#include "Display.hpp"

Display *Display::instance = nullptr;

Display::Display(String imageDir)
    : imageDir(imageDir)
{
    instance = this;
}

void Display::initializeLVGL(Board &board)
{
    Serial0.println("Initialize LVGL");
    lvgl_port_init(board.getLCD(), board.getTouch());
}

void Display::ifAvoidTearing(Board &board)
{
#if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board.getLCD();
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    /**
     * As the anti-tearing feature typically consumes more PSRAM bandwidth, for the ESP32-S3, we need to utilize the
     * "bounce buffer" functionality to enhance the RGB data bandwidth.
     * This feature will consume `bounce_buffer_size * bytes_per_pixel * 2` of SRAM memory.
     */
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB)
    {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif
}

void Display::renderStartupScreen()
{
    Serial0.println("Render startup screen");
    lvgl_port_lock(-1);
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);

    LV_FONT_DECLARE(lv_montserrat_italic_96);
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "LucydDeck");

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_text_font(&style, &lv_montserrat_italic_96);
    lv_obj_add_style(label, &style, 0);

    lv_obj_align(label, LV_ALIGN_CENTER, 0, -400);

    // Simple slide down animation with a bounce
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label);
    lv_anim_set_values(&a, -400, 0);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_bounce);
    lv_anim_start(&a);

    lvgl_port_unlock();
}

void Display::eventHandlerWrapper(lv_event_t *e)
{
    if (instance)
    {
        instance->eventHandler(e);
    }
}

void Display::eventHandler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    ButtonUserData* userData = (ButtonUserData*)lv_obj_get_user_data(obj);

    if (code == LV_EVENT_DELETE) {
        if (userData && userData->iconPath) {
            free(userData->iconPath);
        }
        delete userData;
        return;
    }

    if (buttonCallback && userData)
        buttonCallback(userData->index, code);
}

void Display::renderButtonsFromConfig(const PageConfig &config)
{
    Serial0.println("Render buttons from config");

    uint8_t index = 0;
    uint16_t x, y;

    lvgl_port_lock(-1);
    lv_obj_clean(lv_scr_act());

    uint32_t startMillis = millis();
    y = SPACING_Y + OUTER_MARGIN_Y;
    for (uint8_t row = 0; row < ROWS; row++)
    {
        x = SPACING_X + OUTER_MARGIN_X;
        if (row > 0)
            y += BTN_SIZE + SPACING_Y;
        for (uint8_t col = 0; col < COLS; col++)
        {
            index = row * COLS + col;
            if (col > 0)
                x += BTN_SIZE + SPACING_X;


            auto it = config.find(index);
            if (it != config.end())
            {
                const ButtonConfig &btnCfg = it->second;

                // Allocate a persistent C string for LVGL
                String iconPath = String("S:") + imageDir + btnCfg.imageName + ".png";
                char* iconPathCStr = strdup(iconPath.c_str());

                // Create a transition animation on recolor
                static lv_style_prop_t tr_prop[] = {LV_STYLE_IMG_RECOLOR_OPA, LV_STYLE_PROP_INV};
                static lv_style_transition_dsc_t tr;
                lv_style_transition_dsc_init(&tr, tr_prop, lv_anim_path_ease_out, 150, 0, NULL);

                // Set the style for the button
                static lv_style_t style_pr;
                lv_style_init(&style_pr);
                lv_style_set_img_recolor_opa(&style_pr, LV_OPA_40);
                lv_style_set_img_recolor(&style_pr, lv_color_black());
                lv_style_set_transition(&style_pr, &tr);

                // Create the button
                lv_obj_t *imgBtn = lv_imgbtn_create(lv_scr_act());
                lv_imgbtn_set_src(imgBtn, LV_IMGBTN_STATE_RELEASED, NULL, iconPathCStr, NULL);
                lv_obj_add_style(imgBtn, &style_pr, LV_STATE_PRESSED);
                lv_obj_set_size(imgBtn, BTN_SIZE, BTN_SIZE);
                lv_obj_align(imgBtn, LV_ALIGN_TOP_LEFT, x, y);

                // Add event callback with user data
                ButtonUserData* userData = new ButtonUserData{index, iconPathCStr};
                lv_obj_set_user_data(imgBtn, userData);
                lv_obj_add_event_cb(imgBtn, eventHandlerWrapper, LV_EVENT_ALL, userData);
            }
            else
            {
                // --- Render placeholder rounded square ---
                lv_obj_t *placeholder = lv_obj_create(lv_scr_act());
                lv_obj_set_size(placeholder, BTN_SIZE, BTN_SIZE);
                lv_obj_align(placeholder, LV_ALIGN_TOP_LEFT, x, y);

                static lv_style_t style_placeholder;
                lv_style_init(&style_placeholder);
                lv_style_set_radius(&style_placeholder, 20);
                lv_style_set_bg_color(&style_placeholder, lv_color_hex(0x222222));
                lv_style_set_border_color(&style_placeholder, lv_color_hex(0x444444));
                lv_style_set_border_width(&style_placeholder, 3);
                lv_obj_add_style(placeholder, &style_placeholder, 0);
            }
        }
    }
    uint32_t time = millis() - startMillis;
    Serial0.printf("Image load: %dms \n", time);

    lvgl_port_unlock();
}
