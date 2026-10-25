// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "FileTransferHandler.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            FileTransferHandler::FileTransferHandler(hardware::Storage &storage, config::ConfigManager &configManager,
                                                     core::DeckController &deck)
                : storage(storage), configManager(configManager), deck(deck)
            {
            }

            usb::ErrorCode FileTransferHandler::start(const String &payload)
            {
                return storage.writeFile(payload);
            }

            usb::ErrorCode FileTransferHandler::chunk(const uint8_t *data, size_t len)
            {
                return storage.handleFileChunk(data, len);
            }

            usb::ErrorCode FileTransferHandler::end(const uint8_t *data, size_t len)
            {
                if (len < 4)
                {
                    storage.cancelFileTransfer();
                    return usb::ErrorCode::ERR_CRC_MISSING;
                }

                uint32_t expectedCRC = (static_cast<uint32_t>(data[0]) << 24) |
                                       (static_cast<uint32_t>(data[1]) << 16) |
                                       (static_cast<uint32_t>(data[2]) << 8) |
                                       static_cast<uint32_t>(data[3]);

                usb::ErrorCode error = storage.finishFile(expectedCRC);
                if (error != usb::ErrorCode::OK)
                    return error;

                // The commit only changes what is on the card; the deck keeps the frame it read
                // before it. Repaint the entity the transfer wrote, and nothing else.
                const hardware::Storage::TransferTarget &target = storage.getTransferTarget();
                if (target.type == usb::TargetType::TARGET_ICON)
                {
                    deck.onIconChanged(target.iconName);
                    return usb::ErrorCode::OK;
                }

                configManager.refreshProfile(target.profile);
                if (configManager.isDisplayedPage(target.profile, target.pageId))
                    deck.refreshDisplayedPage();
                return usb::ErrorCode::OK;
            }

            usb::ErrorCode FileTransferHandler::cancel()
            {
                return storage.cancelFileTransfer();
            }
        }
    }
}
