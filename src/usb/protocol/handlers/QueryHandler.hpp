// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include "usb/UsbManager.hpp"
#include "hardware/Storage.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            class QueryHandler
            {
            public:
                QueryHandler(usb::UsbManager &usbManager, hardware::Storage &storage);
                void sendImageList();

            private:
                usb::UsbManager &usbManager;
                hardware::Storage &storage;
            };
        }
    }
}