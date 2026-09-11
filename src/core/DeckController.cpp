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

    usb::ErrorCode DeckController::navigate(const String &payload)
    {
        std::optional<ActionData> parsed = parseActionString(payload);
        if (!parsed.has_value())
            return usb::ErrorCode::ERR_UNKNOWN_ACTION;

        if (std::holds_alternative<PageAction>(*parsed))
        {
            const PageAction &page = std::get<PageAction>(*parsed);
            if (!config.loadPage(page.targetPage))
            {
                Serial0.println("[ERR] Failed to load page");
                return usb::ErrorCode::ERR_PAGE_LOAD;
            }
            render();
            return usb::ErrorCode::OK;
        }

        if (std::holds_alternative<ProfileAction>(*parsed))
        {
            const ProfileAction &profile = std::get<ProfileAction>(*parsed);
            if (!config.loadProfile(profile.targetProfile))
            {
                Serial0.println("[ERR] Failed to load profile");
                return usb::ErrorCode::ERR_PROFILE_LOAD;
            }
            render();
            return usb::ErrorCode::OK;
        }

        return usb::ErrorCode::ERR_UNKNOWN_ACTION;
    }

    bool DeckController::handleInternal(const config::actions::ActionData &action)
    {
        if (std::holds_alternative<PageAction>(action))
        {
            const PageAction &pageAction = std::get<PageAction>(action);
            if (!config.loadPage(pageAction.targetPage))
            {
                Serial0.println("[ERR] Failed to load page");
                return false;
            }
            render();
            return true;
        }

        if (std::holds_alternative<ProfileAction>(action))
        {
            const ProfileAction &profileAction = std::get<ProfileAction>(action);
            if (!config.loadProfile(profileAction.targetProfile))
            {
                Serial0.println("[ERR] Failed to load profile");
                return false;
            }
            render();
            return true;
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
