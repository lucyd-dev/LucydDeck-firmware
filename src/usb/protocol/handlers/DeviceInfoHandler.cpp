// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "DeviceInfoHandler.hpp"
#include <ArduinoJson.h>

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            DeviceInfoHandler::DeviceInfoHandler(usb::UsbManager &usbManager, hardware::Storage &storage)
                : usbManager(usbManager), storage(storage)
            {
            }

            void DeviceInfoHandler::handle(usb::OpCode opCode)
            {
                switch (opCode)
                {
                case usb::CMD_GET_DEVICE_INFO:
                    sendDeviceInfo();
                    break;
                default:
                    break;
                }
            }

            void DeviceInfoHandler::sendDeviceInfo()
            {
                JsonDocument doc;
                JsonObject info = doc.to<JsonObject>();

                info["fw_version"] = FW_VERSION;
                info["protocol_version"] = PROTOCOL_VERSION;
                info["board"] = BOARD_NAME;
                info["free_space_kb"] = storage.freeSpaceKb();

                String response;
                serializeJson(doc, response);
                usbManager.sendPacket(usb::RESP_DEVICE_INFO, response);
            }
        }
    }
}
