// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include "usb/OpCodes.hpp"
#include "usb/Errors.hpp"
#include "hardware/Storage.hpp"
#include "config/ConfigManager.hpp"
#include "core/DeckController.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            // Single-entity deletion: CMD_PAGE_DELETE and CMD_ICON_DELETE. Both take an identity,
            // never a path, and the firmware resolves it to the file it removes.
            class DeleteHandler
            {
            public:
                DeleteHandler(hardware::Storage &storage, config::ConfigManager &configManager,
                              core::DeckController &deck);
                usb::ErrorCode removePage(const String &identity);
                usb::ErrorCode removeIcon(const String &identity);

            private:
                hardware::Storage &storage;
                config::ConfigManager &configManager;
                core::DeckController &deck;
            };
        }
    }
}
