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

    void DeckController::render()
    {
        display.renderButtonsFromConfig(config.getCurrentConfig());
    }
}
