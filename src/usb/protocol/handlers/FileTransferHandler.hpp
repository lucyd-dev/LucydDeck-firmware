// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include "usb/Errors.hpp"
#include "hardware/Storage.hpp"
#include "config/ConfigManager.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            class FileTransferHandler
            {
            public:
                FileTransferHandler(hardware::Storage &storage, config::ConfigManager &configManager);
                usb::ErrorCode start(const String &payload);
                usb::ErrorCode chunk(const uint8_t *data, size_t len);
                usb::ErrorCode end(const uint8_t *data, size_t len);
                usb::ErrorCode cancel();

            private:
                hardware::Storage &storage;
                config::ConfigManager &configManager;
            };
        }
    }
}