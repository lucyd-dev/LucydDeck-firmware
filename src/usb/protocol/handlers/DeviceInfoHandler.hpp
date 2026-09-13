// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include "usb/OpCodes.hpp"
#include "usb/UsbManager.hpp"
#include "hardware/Storage.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            class DeviceInfoHandler
            {
            public:
                DeviceInfoHandler(usb::UsbManager &usbManager, hardware::Storage &storage);
                void handle(OpCode opCode);

            private:
                usb::UsbManager &usbManager;
                hardware::Storage &storage;
                void sendDeviceInfo();
            };
        }
    }
}
