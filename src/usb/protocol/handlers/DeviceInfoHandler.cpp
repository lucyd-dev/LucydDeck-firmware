// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "DeviceInfoHandler.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            DeviceInfoHandler::DeviceInfoHandler(usb::UsbManager &usbManager) : usbManager(usbManager)
            {
            }

            void DeviceInfoHandler::handle(usb::Command command)
            {
                switch (command)
                {
                case usb::CMD_VERSION:
                    usbManager.sendPacket(usb::RESP_VERSION, VERSION, strlen(VERSION));
                    break;
                case usb::CMD_DEVICE_NAME:
                    usbManager.sendPacket(usb::RESP_DEVICE_NAME, DEVICE_NAME, strlen(DEVICE_NAME));
                    break;
                case usb::CMD_BOARD_INFO:
                    usbManager.sendPacket(usb::RESP_BOARD_INFO, BOARD_NAME, strlen(BOARD_NAME));
                    break;
                default:
                    break;
                }
            }
        }
    }
}