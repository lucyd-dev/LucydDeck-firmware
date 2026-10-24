// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "DeleteHandler.hpp"

namespace usb
{
    namespace protocol
    {
        namespace handlers
        {
            DeleteHandler::DeleteHandler(hardware::Storage &storage, config::ConfigManager &configManager,
                                         core::DeckController &deck)
                : storage(storage), configManager(configManager), deck(deck)
            {
            }

            usb::ErrorCode DeleteHandler::removePage(const String &identity)
            {
                hardware::Storage::PageIdentity page;
                usb::ErrorCode error = hardware::Storage::parsePageIdentity(identity, page);
                if (error != usb::ErrorCode::OK)
                    return error;

                if (!storage.dirExists(hardware::Storage::profilePath(page.profile)))
                    return usb::ErrorCode::ERR_PROFILE_NOT_FOUND;

                error = storage.deleteFile(hardware::Storage::pagePath(page));

                // The profiles list is served from the cache, so rebuild it either way: a page
                // that is gone must not stay advertised, and a failed unlink leaves the file
                // in place for the rebuild to pick up again.
                configManager.refreshProfile(page.profile);

                if (error != usb::ErrorCode::OK)
                    return error;

                deck.onPageDeleted(page.profile, page.id);
                return usb::ErrorCode::OK;
            }

            usb::ErrorCode DeleteHandler::removeIcon(const String &identity)
            {
                if (!hardware::Storage::isIconIdentity(identity))
                    return usb::ErrorCode::ERR_INVALID_PATH;

                // The image list is rebuilt from the directory on every request, so nothing to
                // invalidate here. The current page keeps the pixmap it already loaded.
                return storage.deleteFile(hardware::Storage::iconPath(identity));
            }
        }
    }
}
