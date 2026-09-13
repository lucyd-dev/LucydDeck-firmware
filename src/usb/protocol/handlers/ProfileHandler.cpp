// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "ProfileHandler.hpp"
#include <ArduinoJson.h>

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            ProfileHandler::ProfileHandler(usb::UsbManager &usbManager, hardware::Storage &storage,
                                           config::ConfigManager &configManager, core::DeckController &deck)
                : usbManager(usbManager), storage(storage), configManager(configManager), deck(deck)
            {
            }

            usb::ErrorCode ProfileHandler::create(const String &name)
            {
                if (!hardware::Storage::validateName(name))
                    return usb::ErrorCode::ERR_INVALID_PATH;
                if (storage.fileExists(hardware::Storage::PROFILES_DIR + name))
                    return usb::ErrorCode::ERR_PROFILE_EXISTS;

                usb::ErrorCode error = storage.createDir(hardware::Storage::PROFILES_DIR + name);
                if (error == usb::ErrorCode::OK)
                    configManager.addProfile(name);
                return error;
            }

            usb::ErrorCode ProfileHandler::rename(const String &payload)
            {
                String oldName, newName;
                splitString(payload, ':', oldName, newName);
                if (!hardware::Storage::validateName(oldName) || !hardware::Storage::validateName(newName))
                    return usb::ErrorCode::ERR_INVALID_PATH;

                usb::ErrorCode error = storage.renameDir(hardware::Storage::PROFILES_DIR + oldName,
                                                        hardware::Storage::PROFILES_DIR + newName);
                if (error == usb::ErrorCode::OK)
                    deck.onProfileRenamed(oldName, newName);
                return error;
            }

            usb::ErrorCode ProfileHandler::remove(const String &name)
            {
                if (!hardware::Storage::validateName(name))
                    return usb::ErrorCode::ERR_INVALID_PATH;

                usb::ErrorCode error = storage.deleteDirRecursive(hardware::Storage::PROFILES_DIR + name);

                deck.onProfileDeleted(name);
                return error;
            }

            void ProfileHandler::sendProfilesList()
            {
                JsonDocument doc;
                JsonArray profilesList = doc.to<JsonArray>();

                const auto &profiles = configManager.getProfiles();
                for (const auto &profileEntry : profiles)
                {
                    JsonObject profileObj = profilesList.add<JsonObject>();
                    profileObj["name"] = profileEntry.first;

                    JsonArray pages = profileObj["pages"].to<JsonArray>();
                    for (const auto &pageEntry : profileEntry.second)
                    {
                        JsonObject pageObj = pages.add<JsonObject>();
                        pageObj["id"] = pageEntry.first;
                        pageObj["filename"] = pageEntry.second;

                        String fullPath = hardware::Storage::PROFILES_DIR + profileEntry.first + "/" + pageEntry.second;
                        File f = storage.openFile(fullPath);
                        if (f)
                        {
                            pageObj["hash"] = storage.calculateFileCRC(f);
                        }
                        else
                        {
                            pageObj["hash"] = 0;
                        }
                        f.close();
                    }
                }

                String response;
                serializeJson(doc, response);
                usbManager.sendPacket(usb::RESP_PROFILES_LIST, response);
            }

            void ProfileHandler::splitString(const String &input, char delimiter, String &part1, String &part2)
            {
                int delimiterIndex = input.indexOf(delimiter);
                if (delimiterIndex == -1)
                {
                    part1 = input;
                    part2 = "";
                }
                else
                {
                    part1 = input.substring(0, delimiterIndex);
                    part2 = input.substring(delimiterIndex + 1);
                }
            }
        }
    }
}
