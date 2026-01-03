#include "ConfigManager.h"

ConfigManager::ConfigManager(const String &configRoot)
    : configRoot(configRoot)
{
}

void ConfigManager::init()
{
    scanPageDir();

    bool configExists = loadPage();
    if (!configExists)
        Serial0.println("no config found, using blank config");

}

void ConfigManager::scanPageDir()
{
    currentPageId = 0;
    pageList.clear();
    for (uint8_t i = 0; i < 10; i++)
    {
        if (SD.exists(getPageFilename(i)))
        {
            pageList[i] = getPageFilename(i);
        }
    }
    currentPageId = pageList.begin()->first;
    Serial0.printf("Found %d pages\n", pageList.size());
}

const PageConfig &ConfigManager::getCurrentConfig() const
{
    return currentConfig;
}

const PageList &ConfigManager::getPageList() const
{
    return pageList;
}

uint8_t ConfigManager::getPageCount() const
{
    return pageList.size();
}

uint8_t ConfigManager::getCurrentPageId() const
{
    return currentPageId;
}

String ConfigManager::getPageFilename(uint8_t pageId) const
{
    return configRoot + "page-" + String(pageId) + ".json";
}

bool ConfigManager::isPageValid(uint8_t pageId) const
{
    return pageList.find(pageId) != pageList.end();
}

bool ConfigManager::loadPage(uint8_t pageId)
{
    if(pageList.find(pageId) == pageList.end())
        return false;

    String filename = getPageFilename(pageId);
    File file = SD.open(filename, FILE_READ);
    if (!file)
        return false;

    String json;
    while (file.available())
    {
        json += (char)file.read();
    }
    file.close();

    PageConfig newConfig;
    if (!parsePageJson(json, newConfig))
        return false;

    currentConfig = std::move(newConfig);
    currentPageId = pageId;
    return true;
}

bool ConfigManager::parsePageJson(const String &json, PageConfig &outConfig)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err)
        return false;

    JsonObject root = doc.as<JsonObject>();
    if (!root["buttons"].is<JsonObject>())
        return false;

    JsonObject buttons = root["buttons"].as<JsonObject>();
    for (JsonPair kv : buttons)
    {
        ButtonId btnId = atoi(kv.key().c_str());
        JsonObject btnObj = kv.value().as<JsonObject>();
        ButtonConfig btnConfig;

        btnConfig.imageName = btnObj["imageName"].as<String>();

        JsonArray clickSeq = btnObj["click"].as<JsonArray>();
        btnConfig.click = parseActionSequence(clickSeq);

        JsonArray longPressSeq = btnObj["longPress"].as<JsonArray>();
        btnConfig.longPress = parseActionSequence(longPressSeq);

        outConfig[btnId] = btnConfig;
    }
    return true;
}

ActionSequence ConfigManager::parseActionSequence(const JsonArray &seq)
{
    ActionSequence sequence;
    for (JsonObject actionObj : seq)
    {
        String actionType = actionObj["action"].as<String>();
        ActionTypeEnum typeEnum = actionTypeFromString(actionType);

        switch (typeEnum)
        {
        case ActionTypeEnum::HID_KEY:
        {
            HidKeyAction act;
            if (actionObj["keycodes"].is<JsonArray>())
            {
                for (JsonVariant key : actionObj["keycodes"].as<JsonArray>())
                {
                    uint16_t code = getKeyValue(key.as<String>());
                    if (code != 0)
                        act.keycodes.push_back((uint8_t)code);
                }
            }
            sequence.push_back(ActionStep{act});
            break;
        }
        case ActionTypeEnum::CONTROL_KEY:
        {
            ControlKeyAction act;
            act.keycode = getKeyValue(actionObj["keycode"].as<String>());
            sequence.push_back(ActionStep{act});
            break;
        }
        case ActionTypeEnum::MOUSE_MOVE:
        {
            MouseMoveAction act;
            act.x = actionObj["x"] | 0;
            act.y = actionObj["y"] | 0;
            act.wheel = actionObj["wheel"] | 0;
            sequence.push_back(ActionStep{act});
            break;
        }
        case ActionTypeEnum::MOUSE_CLICK:
        {
            MouseClickAction act;
            act.button = actionObj["button"] | "";
            act.count = actionObj["count"] | 1;
            sequence.push_back(ActionStep{act});
            break;
        }
        case ActionTypeEnum::DELAY:
        {
            DelayAction act;
            act.ms = actionObj["ms"] | 0;
            sequence.push_back(ActionStep{act});
            break;
        }
        case ActionTypeEnum::TEXT:
        {
            TextAction act;
            act.text = actionObj["string"] | "";
            sequence.push_back(ActionStep{act});
            break;
        }
        case ActionTypeEnum::CMD:
        {
            CmdAction act;
            act.command = actionObj["command"] | "";
            sequence.push_back(ActionStep{act});
            break;
        }
        case ActionTypeEnum::PAGE:
        {
            PageAction act;
            act.targetPage = actionObj["targetPage"] | 0;
            sequence.push_back(ActionStep{act});
            break;
        }
        default:
            break;
        }
    }
    return sequence;
}
