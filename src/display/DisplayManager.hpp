// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <string>
#include "lvgl/lvgl_v8_port.h"
#include "UIConfig.hpp"
#include "config/Actions.hpp"
#include "hardware/Storage.hpp"


namespace display
{
    struct ButtonUserData
    {
        uint8_t index;
    };

    struct ButtonSlot
    {
        lv_obj_t *btn = nullptr;
        lv_obj_t *img = nullptr;
        lv_obj_t *label = nullptr;
        ButtonUserData userData;
        std::string currentImageBase;
        std::string currentIconPath;
        std::string currentLabel;
        std::string currentBgColor;
        bool hasEmptyStyle = false;
    };

    class DisplayManager
    {
    public:
        static DisplayManager *instance;
        using ButtonCallback = std::function<void(uint8_t, lv_event_code_t)>;
        DisplayManager(hardware::Storage &storage);
        ~DisplayManager() = default;
        void initializeLVGL(esp_panel::board::Board &board);
        static void ifAvoidTearing(esp_panel::board::Board &board);
        void renderStartupScreen();
        void renderButtonsFromConfig(const config::actions::PageConfig &config);
        void setButtonCallback(const ButtonCallback &cb) { buttonCallback = cb; }
        bool isRenderPending() const { return renderPending_; }

    private:
        hardware::Storage &storage;
        ButtonCallback buttonCallback = nullptr;
        lv_style_t style_pr;
        lv_style_t style_btn;
        lv_style_t style_empty;
        lv_style_t style_splash_label;
        lv_obj_t *scrSplash = nullptr;
        lv_obj_t *scrMain = nullptr;
        lv_obj_t *splashLabel = nullptr;
        ButtonSlot slots_[Layout::ROWS * Layout::COLS];
        bool renderPending_ = false;
        uint32_t lastRenderAttemptMs_ = 0;
        static constexpr int LVGL_LOCK_TIMEOUT_MS = 100;
        static constexpr uint32_t RENDER_RETRY_INTERVAL_MS = 200;
        bool lockLvglWithRetry();
        static void eventHandler(lv_event_t *e);
        void setStyles();
        void createSplashScreen();
        void createMainScreen();
        void updateSlot(ButtonSlot &slot, const config::actions::ButtonConfig &btnCfg);
        void setSlotEmpty(ButtonSlot &slot);
        void setEmptyStyle(ButtonSlot &slot, bool apply);
        static bool parseHexColor(const String &hex, lv_color_t &outColor);
        void applySlotBgColor(ButtonSlot &slot, const String &hexColor);
    };
}
