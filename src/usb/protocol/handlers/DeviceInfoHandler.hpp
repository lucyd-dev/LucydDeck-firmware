// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include "usb/Commands.hpp"
#include "usb/UsbManager.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            class DeviceInfoHandler
            {
            public:
                DeviceInfoHandler(usb::UsbManager &usbManager);
                void handle(usb::Command command);

            private:
                usb::UsbManager &usbManager;
            };
        }
    }
}