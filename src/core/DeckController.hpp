// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include "config/ConfigManager.hpp"
#include "display/DisplayManager.hpp"
#include "usb/Errors.hpp"

namespace core
{
    class DeckController
    {
    public:
        DeckController(config::ConfigManager &config, display::DisplayManager &display);
        usb::ErrorCode setActiveProfile(const String &profileName);
        usb::ErrorCode setActivePage(uint8_t pageId);
        bool handleInternal(const config::actions::ActionData &action);
        void onProfileRenamed(const String &oldName, const String &newName);
        void onProfileDeleted(const String &name);
        void render();

    private:
        config::ConfigManager &config;
        display::DisplayManager &display;
    };
}
