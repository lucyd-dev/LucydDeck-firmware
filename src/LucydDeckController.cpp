#include "LucydDeckController.h"

LucydDeckController *LucydDeckController::instance = nullptr;
esp_expander::Base *LucydDeckController::expander = nullptr;
esp_panel::board::Board *LucydDeckController::board = nullptr;

LucydDeckController::LucydDeckController(String cfgRoot, String imageRoot)
    : actionHandler(keyboard, consumer, mouse),
      imageDir("/" + imageRoot),
      configDir("/" + cfgRoot),
      configManager(configDir),
      display(imageRoot)
{
    instance = this;
}

void LucydDeckController::begin()
{
    hidDevice.setPacketCallback(std::bind(&LucydDeckController::onPacketReceived, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    display.setButtonCallback(std::bind(&LucydDeckController::onButtonPress, this, std::placeholders::_1, std::placeholders::_2));

    initBoard();
    initSD();

    keyboard.begin();
    consumer.begin();
    mouse.begin();
    hidDevice.begin();

    configManager.init();

    display.initializeLVGL(*board);
    display.renderStartupScreen();

    delay(800); // Give LVGL time to process the animation

    display.renderButtonsFromConfig(configManager.getCurrentConfig());
}

void LucydDeckController::loop()
{
    hidDevice.loop();
}

void LucydDeckController::initBoard()
{
    Serial0.println("Initialize Board");
    board = new Board();
    board->init();

    display.ifAvoidTearing(*board);

    static_cast<esp_panel::drivers::BusI2C *>(board->getTouch()->getBus())->configI2C_HostSkipInit();
    board->getIO_Expander()->skipInitHost();

    assert(board->begin());

    expander = board->getIO_Expander()->getBase();
    expander->digitalWrite(SD_CS, LOW);
    expander->digitalWrite(USB_SEL, LOW);
}

void LucydDeckController::initSD()
{
    Serial0.println("Initialize SD Card");
    SPI.setHwCs(false);
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_SS);
    SD.begin(SD_CS);

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE)
    {
        Serial0.println("No SD card attached");
        return;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    Serial0.print("SD Card Type: ");
    switch (cardType)
    {
    case CARD_MMC:
        Serial0.println("MMC");
        break;
    case CARD_SD:
        Serial0.println("SDSC");
        break;
    case CARD_SDHC:
        Serial0.println("SDHC");
        break;
    default:
        Serial0.println("UNKNOWN");
        break;
    }

    if (!SD.exists(imageDir))
    {
        Serial0.printf("Creating image dir: %s\n", imageDir);
        if (!SD.mkdir(imageDir))
        {
            Serial0.println("mkdir failed of image dir");
        }
    }

    if (!SD.exists(configDir))
    {
        Serial0.printf("Creating config dir: %s\n", configDir);
        if (!SD.mkdir(configDir))
        {
            Serial0.println("mkdir failed of config dir");
        }
    }
}

void LucydDeckController::onButtonPress(ButtonId id, lv_event_code_t code)
{
    const PageConfig &config = configManager.getCurrentConfig();
    auto it = config.find(id);
    if (it == config.end())
        return;

    const ButtonConfig &btnCfg = it->second;
    const ActionSequence *seq = nullptr;

    if (code == LV_EVENT_SHORT_CLICKED)
    {
        seq = &btnCfg.click;
    }
    else if (code == LV_EVENT_LONG_PRESSED)
    {
        seq = &btnCfg.longPress;
    }

    if (seq)
    {
        for (const auto &step : *seq)
        {
            if (std::holds_alternative<PageAction>(step.data))
            {
                switchPage(std::get<PageAction>(step.data).targetPage);
                break;
            }
            else if (std::holds_alternative<CmdAction>(step.data))
            {
                String command = std::get<CmdAction>(step.data).command;
                hidDevice.sendPacket(CMD_TRIGGER_ACTION, command.c_str(), command.length());
            }
            else
            {
                actionHandler.execute(step);
            }
        }
    }
}

void LucydDeckController::onPacketReceived(Command command, const uint8_t *data, size_t len)
{
    switch (command)
    {
    case CMD_VERSION:
        hidDevice.sendPacket(CMD_VERSION, VERSION, strlen(VERSION));
        break;
    case CMD_CONFIG_UPLOAD:
        openFile(configDir + String(data, len) + ".json");
        break;
    case CMD_IMAGE_UPLOAD:
        openFile(imageDir + String(data, len) + ".png");
        break;
    case CMD_FILE_CHUNK:
        handleFileChunk(data, len);
        break;
    case CMD_FILE_END:
        Serial0.println("File end");
        closeFile();
        break;
    case CMD_PAGE:
        if (configManager.isPageValid(data[2]))
            switchPage(data[2]);
        break;
    case CMD_PAGE_NEXT:
        for (const auto &page : configManager.getPageList())
        {
            if (page.first > configManager.getCurrentPageId())
            {
                switchPage(page.first);
                break;
            }
        }
        break;
    case CMD_PAGE_PREV:
        for (const auto &page : configManager.getPageList())
        {
            if (page.first < configManager.getCurrentPageId())
            {
                switchPage(page.first);
                break;
            }
        }
        break;
    default:
        break;
    }
}

void LucydDeckController::sendAck()
{
    Serial0.println("Sending ack");
    hidDevice.sendPacket(CMD_ACK, "", 0);
}

void LucydDeckController::sendError(const String &error)
{
    hidDevice.sendPacket(CMD_ERROR, error.c_str(), error.length());
}

void LucydDeckController::handleFileChunk(const uint8_t *data, size_t len)
{
    if (len > FILE_CHUNK_MAX_SIZE)
    {
        Serial0.println("[ERR] Chunk too large for fileQueue");
        sendError("chunk_too_large");
        return;
    }

    if (!receivingFile || !file)
    {
        sendError("no_file_open");
        return;
    }

    size_t written = file.write(data, len);
    if (written != len)
    {
        Serial0.println("[ERR] Failed to write to file");
        sendError("file_write_error");
        receivingFile = false;
        closeFile();
        return;
    }
    sendAck();
}

void LucydDeckController::openFile(const String &path)
{
    if (receivingFile)
    {
        Serial0.println("[ERR] Already receiving file");
        sendError("transfer_in_progress");
        return;
    }

    if (file)
        file.close();

    file = SD.open(path.c_str(), FILE_WRITE);
    if (!file)
    {
        Serial0.printf("[ERR] Failed to open file: %s\n", path.c_str());
        sendError("file_write_error");
        return;
    }

    Serial0.printf("Creating new file: %s\n", path.c_str());
    receivingFile = true;
    sendAck();
}

void LucydDeckController::closeFile()
{
    if (file)
    {
        file.close();
        file = File();
        receivingFile = false;
    }
    sendAck();
    Serial0.println("File saved successfully");
}

void LucydDeckController::switchPage(uint8_t pageId)
{
    Serial0.printf("Switching to page: %d\n", pageId);
    if (configManager.loadPage(pageId))
    {
        display.renderButtonsFromConfig(configManager.getCurrentConfig());
    }
    else
    {
        Serial0.println("[ERR] Failed to load page");
        sendError("page_load_error");
    }
    sendAck();
}
