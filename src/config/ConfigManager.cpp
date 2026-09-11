// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "ConfigManager.hpp"
#include <algorithm>

namespace config
{
    using namespace actions;

    ConfigManager::ConfigManager(hardware::Storage &storage) : storage(storage)
    {
    }

    void ConfigManager::begin()
    {
        storage.createDir(hardware::Storage::PROFILES_DIR);
        scanProfileDir();

        if (profiles.empty()) {
            Serial0.println("No profiles found. Creating default profile...");
            storage.createDir(hardware::Storage::PROFILES_DIR + String("default"));
            scanProfileDir();
        }

        if (!ensureProfileSelected())
            Serial0.println("no config found, using blank config");
    }
    
    bool ConfigManager::loadPage(uint8_t pageId)
    {
        if (currentProfile.isEmpty() || profiles.find(currentProfile) == profiles.end()) {
            Serial0.println("Cannot load page: No valid profile selected");
            return false;
        }

        PageConfig newConfig;
        if (!loadPageFromFile(currentProfile, pageId, newConfig))
            return false;

        currentConfig = std::move(newConfig);
        return true;
    }
    
    bool ConfigManager::loadProfile(const String &profileName)
    {
        if (!hardware::Storage::validateName(profileName))
            return false;

        auto it = profiles.find(profileName);
        if (it == profiles.end())
            return false;

        if (it->second.empty())
            return false;

        uint8_t pageId = it->second.begin()->first;
        PageConfig newConfig;
        if (!loadPageFromFile(profileName, pageId, newConfig))
            return false;

        currentProfile = profileName;
        currentConfig = std::move(newConfig);
        return true;
    }

    bool ConfigManager::loadPageFromFile(const String &profileName, uint8_t pageId, PageConfig &outConfig)
    {
        auto profileIt = profiles.find(profileName);
        if (profileIt == profiles.end())
            return false;

        auto pageIt = profileIt->second.find(pageId);
        if (pageIt == profileIt->second.end())
            return false;

        String fullPath = hardware::Storage::PROFILES_DIR + profileName + "/" + pageIt->second;

        if (!storage.fileExists(fullPath))
        {
            Serial0.printf("Page file not found: %s\n", fullPath.c_str());
            return false;
        }

        File file = storage.openFile(fullPath);
        if (!file)
        {
            Serial0.printf("Failed to open page file: %s\n", fullPath.c_str());
            return false;
        }

        bool success = parsePageJson(file, outConfig);
        file.close();
        return success;
    }

    void ConfigManager::scanProfileDir()
    {
        std::vector<String> profilesList = storage.listDir(hardware::Storage::PROFILES_DIR, true);
        
        for (const String& profile : profilesList)
        {
            if (profiles.find(profile) != profiles.end())
                continue;

            profiles[profile] = PageList();
            Serial0.printf("Found profile: %s\n", profile.c_str());

            profiles[profile] = buildPageList(hardware::Storage::PROFILES_DIR + profile + "/");
        }
    }

    PageList ConfigManager::buildPageList(const String &pageDir)
    {
        PageList result;
        std::vector<String> pagesList = storage.listDir(pageDir, false);
        std::sort(pagesList.begin(), pagesList.end());

        for (const String &page : pagesList)
        {
            std::optional<uint8_t> pageId = parsePageId(page);
            if (!pageId.has_value())
            {
                Serial0.printf("  Skipping non-page file: %s\n", page.c_str());
                continue;
            }
            if (result.find(*pageId) == result.end())
            {
                result[*pageId] = page;
                Serial0.printf("  Found page: %s (id %u)\n", page.c_str(), *pageId);
            }
        }
        return result;
    }

    std::optional<uint8_t> ConfigManager::parsePageId(const String &filename)
    {
        return hardware::Storage::parsePageId(filename);
    }

    bool ConfigManager::ensureProfileSelected()
    {
        if (profiles.empty())
        {
            currentProfile = "";
            currentConfig.clear();
            Serial0.println("No profiles available, using blank config");
            return false;
        }

        if (currentProfile.isEmpty() || profiles.find(currentProfile) == profiles.end())
        {
            if (profiles.count("default"))
                currentProfile = "default";
            else
                currentProfile = profiles.begin()->first;
        }

        if (!loadProfile(currentProfile))
        {
            Serial0.println("no config found, using blank config");
            return false;
        }

        Serial0.printf("Selected profile: %s\n", currentProfile.c_str());
        return true;
    }

    void ConfigManager::refreshProfile(const String &profileName)
    {
        auto it = profiles.find(profileName);
        if (it == profiles.end())
            return;

        it->second = buildPageList(hardware::Storage::PROFILES_DIR + profileName + "/");
    }

    void ConfigManager::addProfile(const String &name)
    {
        if (profiles.find(name) == profiles.end())
            profiles[name] = PageList();
    }

    void ConfigManager::onProfileRenamed(const String &oldName, const String &newName)
    {
        auto it = profiles.find(oldName);
        if (it == profiles.end())
            return;

        PageList pages = std::move(it->second);
        profiles.erase(oldName);
        profiles[newName] = std::move(pages);

        if (currentProfile == oldName)
            currentProfile = newName;
    }

    void ConfigManager::onProfileDeleted(const String &name)
    {
        profiles.erase(name);
        if (currentProfile == name)
            currentProfile = "";
    }

    const ProfileList &ConfigManager::getProfiles() const
    {
        return profiles;
    }

    const String &ConfigManager::getCurrentProfile() const
    {
        return currentProfile;
    }

    const PageConfig &ConfigManager::getCurrentConfig() const
    {
        return currentConfig;
    }

    bool ConfigManager::parsePageJson(Stream &json, PageConfig &outConfig)
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
            uint8_t btnId = atoi(kv.key().c_str());
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
        size_t dropped = 0;

        for (JsonVariant item : seq)
        {
            if (!item.is<const char*>())
            {
                dropped++;
                continue;
            }

            std::optional<ActionData> parsed = parseActionString(item.as<String>());
            if (parsed.has_value())
            {
                sequence.push_back(ActionStep{std::move(*parsed)});
            }
            else
            {
                dropped++;
            }
        }
        if (dropped > 0)
        {
            Serial0.printf("[WARN] %u action(s) skipped (malformed action string, re-upload config)\n", dropped);
        }
        return sequence;
    }
}
