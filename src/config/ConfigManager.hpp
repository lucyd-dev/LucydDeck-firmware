#pragma once

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "usb/helpers/Actions.hpp"
#include "usb/helpers/keyMappings.hpp"

typedef std::map<uint8_t, String> PageList;

class ConfigManager
{
public:
    ConfigManager(const String &configRoot = "/");
    void init();
    void scanPageDir();
    bool loadPage(uint8_t pageId = 0);
    const PageConfig &getCurrentConfig() const;
    const PageList &getPageList() const;
    uint8_t getPageCount() const;
    uint8_t getCurrentPageId() const;
    bool isPageValid(uint8_t pageId) const;

private:
    String configRoot;
    PageConfig currentConfig;
    uint8_t currentPageId;
    PageList pageList;


    bool parsePageJson(const String &json, PageConfig &outConfig);
    ActionSequence parseActionSequence(const JsonArray &seq);
    String getPageFilename(uint8_t pageId) const;
};
