// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>

namespace display
{
    struct Layout
    {
        static constexpr uint16_t SCREEN_W = 800;
        static constexpr uint16_t SCREEN_H = 480;
        static constexpr uint8_t ROWS = 3;
        static constexpr uint8_t COLS = 5;
        static constexpr uint8_t BTN_SZ = 140;
        static constexpr uint8_t EDGE_PAD = 15;

        static constexpr uint8_t COL_GAP = (SCREEN_W - (2 * EDGE_PAD) - (COLS * BTN_SZ)) / (COLS - 1);
        static constexpr uint8_t ROW_GAP = (SCREEN_H - (2 * EDGE_PAD) - (ROWS * BTN_SZ)) / (ROWS - 1);
    };
}
