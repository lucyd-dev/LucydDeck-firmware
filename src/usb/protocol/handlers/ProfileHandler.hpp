// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include "usb/Commands.hpp"
#include "usb/Errors.hpp"
#include "usb/UsbManager.hpp"
#include "hardware/Storage.hpp"
#include "config/ConfigManager.hpp"
#include "core/DeckController.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            class ProfileHandler
            {
            public:
                ProfileHandler(usb::UsbManager &usbManager, hardware::Storage &storage,
                               config::ConfigManager &configManager, core::DeckController &deck);
                usb::ErrorCode create(const String &name);
                usb::ErrorCode rename(const String &payload);
                usb::ErrorCode remove(const String &name);
                void sendProfilesList();

            private:
                usb::UsbManager &usbManager;
                hardware::Storage &storage;
                config::ConfigManager &configManager;
                core::DeckController &deck;

                void splitString(const String &input, char delimiter, String &part1, String &part2);
            };
        }
    }
}