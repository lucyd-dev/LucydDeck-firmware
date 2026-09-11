// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "FileTransferHandler.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            FileTransferHandler::FileTransferHandler(hardware::Storage &storage, config::ConfigManager &configManager)
                : storage(storage), configManager(configManager)
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
                if (error == usb::ErrorCode::OK)
                {
                    const String &profile = storage.getTransferProfile();
                    if (!profile.isEmpty())
                        configManager.refreshProfile(profile);
                }
                return error;
            }

            usb::ErrorCode FileTransferHandler::cancel()
            {
                return storage.cancelFileTransfer();
            }
        }
    }
}
