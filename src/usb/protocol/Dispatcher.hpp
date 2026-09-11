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
#include "handlers/DeviceInfoHandler.hpp"
#include "handlers/ProfileHandler.hpp"
#include "handlers/FileTransferHandler.hpp"
#include "handlers/QueryHandler.hpp"

namespace usb
{
    namespace protocol
    {
        class Dispatcher
        {
        public:
            Dispatcher(usb::UsbManager &usbManager, hardware::Storage &storage,
                       config::ConfigManager &configManager, core::DeckController &deck);
            void onPacket(usb::Command command, const uint16_t sequence, const uint8_t *data, size_t len);

        private:
            usb::UsbManager &usbManager;
            hardware::Storage &storage;
            config::ConfigManager &configManager;
            core::DeckController &deck;
            handlers::DeviceInfoHandler deviceInfo;
            handlers::ProfileHandler profiles;
            handlers::FileTransferHandler fileTransfer;
            handlers::QueryHandler queries;

            void reply(usb::ErrorCode error);
            String payloadString(const uint8_t *data, size_t len);
        };
    }
}
