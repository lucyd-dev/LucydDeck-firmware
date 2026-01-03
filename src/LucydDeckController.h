#pragma once

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include <esp_io_expander.hpp>
#include <esp_display_panel.hpp>
#include "display/Display.h"

#include "usb/helpers/Actions.h"
#include "usb/ActionHandler.h"
#include "config/ConfigManager.h"

#include "usb/CustomHIDDevice.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDMouse.h"

constexpr uint8_t FILE_QUEUE_LENGTH = 8;
constexpr uint8_t FILE_CHUNK_MAX_SIZE = 64;

struct FileChunk
{
    uint8_t data[FILE_CHUNK_MAX_SIZE];
    size_t len;
};

class LucydDeckController
{
public:
    static LucydDeckController *instance;
    LucydDeckController(String cfgRoot, String imageRoot);
    void begin();
    void loop();

    void onButtonPress(ButtonId id, lv_event_code_t code);
    void onPacketReceived(Command command, const uint8_t *data, size_t len);
    void sendAck();
    void sendError(const String &error);
    void openFile(const String &path);
    void handleFileChunk(const uint8_t *data, size_t len);
    void closeFile();

private:
    QueueHandle_t fileQueue;
    static esp_expander::Base *expander;
    static esp_panel::board::Board *board;
    String imageDir;
    String configDir;
    File file;
    bool receivingFile = false;
    Display display;
    USBHIDKeyboard keyboard;
    USBHIDConsumerControl consumer;
    USBHIDMouse mouse;
    CustomHIDDevice hidDevice;
    ActionHandler actionHandler;
    ConfigManager configManager;
    uint8_t currentPage = 0;

    void initBoard();
    void initSD();
    void switchPage(uint8_t pageId);
};
