// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "QueryHandler.hpp"
#include <ArduinoJson.h>

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            QueryHandler::QueryHandler(usb::UsbManager &usbManager, hardware::Storage &storage)
                : usbManager(usbManager), storage(storage)
            {
            }

            void QueryHandler::sendImageList()
            {
                JsonDocument doc;
                JsonArray imagesList = doc.to<JsonArray>();

                std::vector<String> files = storage.listDir(hardware::Storage::IMAGE_DIR, false);
                for (const String &filename : files)
                {
                    JsonObject imageObj = imagesList.add<JsonObject>();
                    imageObj["filename"] = filename;

                    String fullPath = hardware::Storage::IMAGE_DIR + filename;
                    File f = storage.openFile(fullPath);
                    if (f)
                    {
                        imageObj["hash"] = storage.calculateFileCRC(f);
                        f.close();
                    }
                    else
                    {
                        imageObj["hash"] = 0;
                    }
                }

                String response;
                serializeJson(doc, response);
                usbManager.sendPacket(usb::RESP_IMAGES_LIST, response);
            }
        }
    }
}
