// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "Dispatcher.hpp"

namespace usb
{
    namespace protocol
    {
        Dispatcher::Dispatcher(usb::UsbManager &usbManager, hardware::Storage &storage,
                               config::ConfigManager &configManager, core::DeckController &deck)
            : usbManager(usbManager), storage(storage), configManager(configManager), deck(deck),
              deviceInfo(usbManager),
              profiles(usbManager, storage, configManager, deck),
              fileTransfer(storage, configManager),
              queries(usbManager, storage)
        {
        }

        void Dispatcher::onPacket(usb::Command command, const uint16_t sequence, const uint8_t *data, size_t len)
        {
            (void)sequence;

            switch (command)
            {
            case usb::CMD_VERSION:
            case usb::CMD_DEVICE_NAME:
            case usb::CMD_BOARD_INFO:
                deviceInfo.handle(command);
                break;
            case usb::CMD_IMAGE_LIST:
                queries.sendImageList();
                break;
            case usb::CMD_PROFILES_LIST:
                profiles.sendProfilesList();
                break;
            case usb::CMD_PROFILE_CREATE:
                reply(profiles.create(payloadString(data, len)));
                break;
            case usb::CMD_PROFILE_RENAME:
                reply(profiles.rename(payloadString(data, len)));
                break;
            case usb::CMD_PROFILE_DELETE:
                reply(profiles.remove(payloadString(data, len)));
                break;
            case usb::CMD_FILE_START:
                reply(fileTransfer.start(payloadString(data, len)));
                break;
            case usb::CMD_FILE_CHUNK:
                reply(fileTransfer.chunk(data, len));
                break;
            case usb::CMD_FILE_END:
                reply(fileTransfer.end(data, len));
                break;
            case usb::CMD_FILE_CANCEL:
                reply(fileTransfer.cancel());
                break;
            case usb::CMD_NAVIGATE:
                reply(deck.navigate(payloadString(data, len)));
                break;
            default:
                reply(usb::ErrorCode::ERR_UNKNOWN_COMMAND);
                break;
            }
        }

        void Dispatcher::reply(usb::ErrorCode error)
        {
            if (error == usb::ErrorCode::OK)
                usbManager.sendAck();
            else
                usbManager.sendError(error);
        }

        String Dispatcher::payloadString(const uint8_t *data, size_t len)
        {
            size_t strLen = 0;
            while (strLen < len && data[strLen] != 0)
                ++strLen;
            return String(reinterpret_cast<const char *>(data), strLen);
        }
    }
}
