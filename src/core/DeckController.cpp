// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "DeckController.hpp"
#include <variant>

namespace core
{
    using namespace config::actions;

    DeckController::DeckController(config::ConfigManager &config, display::DisplayManager &display)
        : config(config), display(display)
    {
    }

    usb::ErrorCode DeckController::setActiveProfile(const String &profileName)
    {
        if (!config.loadProfile(profileName))
        {
            Serial0.println("[ERR] Failed to load profile");
            return usb::ErrorCode::ERR_PROFILE_LOAD;
        }
        render();
        return usb::ErrorCode::OK;
    }

    usb::ErrorCode DeckController::setActivePage(uint8_t pageId)
    {
        if (!config.loadPage(pageId))
        {
            Serial0.println("[ERR] Failed to load page");
            return usb::ErrorCode::ERR_PAGE_LOAD;
        }
        render();
        return usb::ErrorCode::OK;
    }

    void DeckController::refreshDisplayedPage()
    {
        // The page on screen was read before the transfer. Reload it and repaint; a page that no
        // longer parses leaves the frame that is already up.
        if (!config.reloadDisplayedPage())
        {
            Serial0.println("[WARN] Displayed page not reloaded, keeping the current frame");
            return;
        }
        render();
    }

    void DeckController::onIconChanged(const String &iconName)
    {
        // A new, replaced or deleted icon only reaches the screen once the decoded bitmap for that
        // path is dropped and the slot is told to set its source again.
        display.invalidateIcon(iconName);
        render();
    }

    bool DeckController::handleInternal(const config::actions::ActionData &action)
    {
        if (std::holds_alternative<PageAction>(action))
        {
            const PageAction &pageAction = std::get<PageAction>(action);
            return setActivePage(pageAction.targetPage) == usb::ErrorCode::OK;
        }

        if (std::holds_alternative<ProfileAction>(action))
        {
            const ProfileAction &profileAction = std::get<ProfileAction>(action);
            return setActiveProfile(profileAction.targetProfile) == usb::ErrorCode::OK;
        }

        return false;
    }

    void DeckController::onProfileRenamed(const String &oldName, const String &newName)
    {
        bool wasActive = (config.getCurrentProfile() == oldName);
        config.onProfileRenamed(oldName, newName);
        if (wasActive)
            render();
    }

    void DeckController::onProfileDeleted(const String &name)
    {
        bool wasActive = (config.getCurrentProfile() == name);
        config.onProfileDeleted(name);
        if (wasActive)
        {
            config.ensureProfileSelected();
            render();
        }
    }

    void DeckController::onPageDeleted(const String &profileName, uint8_t pageId)
    {
        // Only the displayed page forces a repaint; deleting any other page leaves the screen
        // untouched. The new content is the lowest remaining page, or a blank deck.
        if (!config.isDisplayedPage(profileName, pageId))
            return;

        config.loadFirstRemainingPage();
        render();
    }

    void DeckController::render()
    {
        display.renderButtonsFromConfig(config.getCurrentConfig());
    }
}
