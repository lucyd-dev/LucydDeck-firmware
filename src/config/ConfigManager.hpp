// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <optional>
#include "hardware/Storage.hpp"
#include "Actions.hpp"

namespace config
{
    class ConfigManager
    {
    public:
        ConfigManager(hardware::Storage &storage);
        ~ConfigManager() = default;
        void begin();
        bool loadPage(uint8_t pageId = 0);
        bool loadProfile(const String &profileName);
        const actions::ProfileList &getProfiles() const;
        const String &getCurrentProfile() const;
        const actions::PageConfig &getCurrentConfig() const;
        void refreshProfile(const String &profileName);
        bool ensureProfileSelected();
        void addProfile(const String &name);
        void onProfileRenamed(const String &oldName, const String &newName);
        void onProfileDeleted(const String &name);
        
        private:
        hardware::Storage &storage;
        actions::ProfileList profiles;
        actions::PageConfig currentConfig;
        String currentProfile;
        
        void scanProfileDir();
        std::optional<uint8_t> parsePageId(const String &filename);
        actions::PageList buildPageList(const String &pageDir);
        bool parsePageJson(Stream &json, actions::PageConfig &outConfig);
        actions::ActionSequence parseActionSequence(const JsonArray &seq);
        bool loadPageFromFile(const String &profileName, uint8_t pageId, actions::PageConfig &outConfig);
    };
}
