// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "DisplayManager.hpp"
#include "hardware/Storage.hpp"
#include <USB.h>

namespace display
{

    DisplayManager *DisplayManager::instance = nullptr;

    bool DisplayManager::lockLvglWithRetry()
    {
        return lvgl_port_lock(LVGL_LOCK_TIMEOUT_MS);
    }

    DisplayManager::DisplayManager(hardware::Storage &storage) : storage(storage)
    {
        instance = this;
    }

    void DisplayManager::initializeLVGL(esp_panel::board::Board &board)
    {
        storage.createDir(hardware::Storage::IMAGE_DIR);

        Serial0.println("Initialize LVGL");
        lvgl_port_init(board.getLCD(), board.getTouch());
        
        uint16_t width = board.getLCD()->getFrameWidth();
        uint16_t height = board.getLCD()->getFrameHeight();
        Serial0.printf("Panel resolution: %dx%d\n", width, height);

        if (!lvgl_port_lock(-1))
        {
            Serial0.println("[ERR] Failed to lock LVGL for screen creation");
            return;
        }
        setStyles();
        createSplashScreen();
        createMainScreen();
        lv_scr_load(scrSplash);
        lvgl_port_unlock();
    }
    
    void DisplayManager::setStyles()
    {
        static lv_style_prop_t tr_prop[] = {LV_STYLE_IMG_RECOLOR_OPA, LV_STYLE_PROP_INV};
        static lv_style_transition_dsc_t tr;
        
        // Style for empty buttons
        lv_style_init(&style_empty);
        lv_style_set_radius(&style_empty, 10);
        lv_style_set_bg_opa(&style_empty, LV_OPA_COVER);
        lv_style_set_bg_color(&style_empty, lv_color_hex(0x222222));
        lv_style_set_border_color(&style_empty, lv_color_hex(0x444444));
        lv_style_set_border_width(&style_empty, 3);
        
        // Style for pressed state of image buttons
        lv_style_init(&style_pr);
        lv_style_set_img_recolor_opa(&style_pr, LV_OPA_40);
        lv_style_set_img_recolor(&style_pr, lv_color_black());
        lv_style_transition_dsc_init(&tr, tr_prop, lv_anim_path_ease_out, 150, 0, NULL);
        lv_style_set_transition(&style_pr, &tr);

        // Style for the "LucydDeck" splash label
        lv_style_init(&style_splash_label);
        lv_style_set_text_color(&style_splash_label, lv_color_white());
        lv_style_set_text_font(&style_splash_label, &lv_montserrat_italic_96);
    }

    void DisplayManager::ifAvoidTearing(esp_panel::board::Board &board)
    {
#if LVGL_PORT_AVOID_TEARING_MODE
        auto lcd = board.getLCD();
        lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
        auto lcd_bus = lcd->getBus();
        if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB)
        {
            static_cast<esp_panel::drivers::BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
        }
#endif
#endif
    }

    void DisplayManager::createSplashScreen()
    {
        scrSplash = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(scrSplash, lv_color_hex(0x000000), LV_PART_MAIN);

        lv_obj_t *label = lv_label_create(scrSplash);
        lv_label_set_text(label, "LucydDeck");
        lv_obj_add_style(label, &style_splash_label, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -400);

        splashLabel = label;
    }

    void DisplayManager::createMainScreen()
    {
        scrMain = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(scrMain, lv_color_hex(0x000000), LV_PART_MAIN);

        static lv_coord_t col_dsc[] = {Layout::BTN_SZ, Layout::BTN_SZ, Layout::BTN_SZ, Layout::BTN_SZ, Layout::BTN_SZ, LV_GRID_TEMPLATE_LAST};
        static lv_coord_t row_dsc[] = {Layout::BTN_SZ, Layout::BTN_SZ, Layout::BTN_SZ, LV_GRID_TEMPLATE_LAST};
        
        lv_obj_t *cont = lv_obj_create(scrMain);
        lv_obj_set_size(cont, Layout::SCREEN_W, Layout::SCREEN_H);
        lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_center(cont);
        lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);
        lv_obj_set_layout(cont, LV_LAYOUT_GRID);
        
        lv_obj_set_style_bg_color(cont, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_border_width(cont, 0, 0);
        
        lv_obj_set_style_pad_all(cont, Layout::EDGE_PAD, 0);
        lv_obj_set_style_pad_column(cont, Layout::COL_GAP, 0);
        lv_obj_set_style_pad_row(cont, Layout::ROW_GAP, 0);
        lv_obj_set_grid_align(cont, LV_GRID_ALIGN_CENTER, LV_GRID_ALIGN_CENTER);

        for (uint8_t row = 0; row < Layout::ROWS; row++)
        {
            for (uint8_t col = 0; col < Layout::COLS; col++)
            {
                uint8_t index = row * Layout::COLS + col;
                ButtonSlot &slot = slots_[index];

                lv_obj_t *btn = lv_imgbtn_create(cont);
                lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
                lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_style(btn, &style_pr, LV_STATE_PRESSED);
                lv_obj_add_style(btn, &style_empty, LV_PART_MAIN);
                
                slot.btn = btn;
                slot.img = btn;
                
                slot.label = lv_label_create(slot.btn);
                lv_label_set_text(slot.label, "");
                lv_obj_set_style_text_color(slot.label, lv_color_white(), 0);
                lv_obj_clear_flag(slot.label, LV_OBJ_FLAG_CLICKABLE);
                
                slot.userData.index = index;
                slot.hasEmptyStyle = true;
                
                lv_obj_set_user_data(btn, &slot.userData);
                lv_obj_add_event_cb(btn, eventHandler, LV_EVENT_ALL, &slot.userData);
            }
        }
    }

    void DisplayManager::renderStartupScreen()
    {
        if (!scrSplash)
            return;

        uint32_t now = millis();
        if (renderPending_ && now - lastRenderAttemptMs_ < RENDER_RETRY_INTERVAL_MS)
            return;
        lastRenderAttemptMs_ = now;

        if (!lockLvglWithRetry())
        {
            renderPending_ = true;
            return;
        }
        renderPending_ = false;
        
        lv_anim_del(splashLabel, NULL);
        lv_obj_align(splashLabel, LV_ALIGN_CENTER, 0, -400);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, splashLabel);
        lv_anim_set_values(&a, -400, 0);
        lv_anim_set_time(&a, 1000);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_path_cb(&a, lv_anim_path_bounce);
        lv_anim_start(&a);

        if (lv_scr_act() != scrSplash)
            lv_scr_load(scrSplash);
        lvgl_port_unlock();
    }

    void DisplayManager::eventHandler(lv_event_t *e)
    {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t *obj = lv_event_get_target(e);
        ButtonUserData *userData = reinterpret_cast<ButtonUserData *>(lv_obj_get_user_data(obj));

        if (instance->buttonCallback && userData)
            instance->buttonCallback(userData->index, code);
    }

    void DisplayManager::renderButtonsFromConfig(const config::actions::PageConfig &config)
    {
        if (!scrMain)
            return;

        uint32_t now = millis();
        if (renderPending_ && now - lastRenderAttemptMs_ < RENDER_RETRY_INTERVAL_MS)
            return;
        lastRenderAttemptMs_ = now;

        if (!lockLvglWithRetry())
        {
            renderPending_ = true;
            return;
        }
        renderPending_ = false;

        for (uint8_t i = 0; i < Layout::ROWS * Layout::COLS; i++)
        {
            auto it = config.find(i);
            if (it == config.end())
                setSlotEmpty(slots_[i]);
            else
                updateSlot(slots_[i], it->second);
        }

        if (lv_scr_act() != scrMain)
            lv_scr_load(scrMain);
        lvgl_port_unlock();
    }

    void DisplayManager::updateSlot(ButtonSlot &slot, const config::actions::ButtonConfig &btnCfg)
    {
        bool hasImage = !btnCfg.imageName.isEmpty();
        bool hasLabel = !btnCfg.label.isEmpty();

        if (hasImage)
        {
            if (slot.currentImageBase != btnCfg.imageName.c_str())
            {
                slot.currentImageBase = btnCfg.imageName.c_str();
                String iconPath = String("S:") + hardware::Storage::IMAGE_DIR + btnCfg.imageName + ".png";
                slot.currentIconPath = iconPath.c_str();
                lv_imgbtn_set_src(slot.img, LV_IMGBTN_STATE_RELEASED, NULL, slot.currentIconPath.c_str(), NULL);
            }
            setEmptyStyle(slot, false);
        }
        else
        {
            if (!slot.currentImageBase.empty())
            {
                slot.currentImageBase.clear();
                slot.currentIconPath.clear();
                lv_imgbtn_set_src(slot.img, LV_IMGBTN_STATE_RELEASED, NULL, NULL, NULL);
            }
            setEmptyStyle(slot, true);
        }

        if (hasLabel)
        {
            if (slot.currentLabel != btnCfg.label.c_str())
            {
                slot.currentLabel = btnCfg.label.c_str();
                lv_label_set_text(slot.label, btnCfg.label.c_str());
                lv_obj_clear_flag(slot.label, LV_OBJ_FLAG_HIDDEN);
                lv_obj_align(slot.label, hasImage ? LV_ALIGN_BOTTOM_MID : LV_ALIGN_CENTER, 0, hasImage ? -6 : 0);
            }
        }
        else
        {
            if (!slot.currentLabel.empty())
            {
                slot.currentLabel.clear();
                lv_label_set_text(slot.label, "");
            }
            lv_obj_add_flag(slot.label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void DisplayManager::setSlotEmpty(ButtonSlot &slot)
    {
        if (!slot.currentImageBase.empty())
        {
            slot.currentImageBase.clear();
            slot.currentIconPath.clear();
            lv_imgbtn_set_src(slot.img, LV_IMGBTN_STATE_RELEASED, NULL, NULL, NULL);
        }
        setEmptyStyle(slot, true);

        if (!slot.currentLabel.empty())
        {
            slot.currentLabel.clear();
            lv_label_set_text(slot.label, "");
        }
        lv_obj_add_flag(slot.label, LV_OBJ_FLAG_HIDDEN);
    }

    void DisplayManager::setEmptyStyle(ButtonSlot &slot, bool apply)
    {
        if (apply == slot.hasEmptyStyle)
            return;
        if (apply)
            lv_obj_add_style(slot.btn, &style_empty, LV_PART_MAIN);
        else
            lv_obj_remove_style(slot.btn, &style_empty, LV_PART_MAIN);
        slot.hasEmptyStyle = apply;
    }
}
