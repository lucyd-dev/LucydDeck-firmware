// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include <esp_io_expander.hpp>
#include <esp_display_panel.hpp>

namespace hardware
{

    class Board
    {
    public:
        Board();
        ~Board();
        void init();
        void begin();
        esp_panel::board::Board &getBoard() { return *board; };

    private:
        static esp_expander::Base *expander;
        static esp_panel::board::Board *board;
        void initExpander();
    };
}
