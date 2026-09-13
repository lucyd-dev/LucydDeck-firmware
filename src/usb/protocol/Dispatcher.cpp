// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "Dispatcher.hpp"
#include "config/Actions.hpp"

namespace usb
{
    namespace protocol
    {
        Dispatcher::Dispatcher(usb::UsbManager &usbManager, hardware::Storage &storage,
                               config::ConfigManager &configManager, core::DeckController &deck)
            : usbManager(usbManager), storage(storage), configManager(configManager), deck(deck),
              deviceInfo(usbManager, storage),
              profiles(usbManager, storage, configManager, deck),
              fileTransfer(storage, configManager),
              queries(usbManager, storage)
        {
        }

        void Dispatcher::onPacket(OpCode opCode, const uint16_t sequence, const uint8_t *data, size_t len)
        {
            (void)sequence;

            switch (opCode)
            {
            case usb::CMD_PING:
                usbManager.sendAck();
                break;
            case usb::CMD_GET_DEVICE_INFO:
                deviceInfo.handle(opCode);
                break;
            case usb::CMD_GET_IMAGES_LIST:
                queries.sendImageList();
                break;
            case usb::CMD_GET_PROFILES_LIST:
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
            case usb::CMD_SET_ACTIVE_PROFILE:
                reply(deck.setActiveProfile(payloadString(data, len)));
                break;
            case usb::CMD_SET_ACTIVE_PAGE:
            {
                std::optional<uint8_t> pageId = config::actions::parsePageTarget(payloadString(data, len));
                if (!pageId.has_value())
                    reply(usb::ErrorCode::ERR_UNKNOWN_ACTION);
                else
                    reply(deck.setActivePage(*pageId));
                break;
            }
            default:
                Serial0.printf("[WARN] Unknown command received: 0x%02X\n", static_cast<uint8_t>(opCode));
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
