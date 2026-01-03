#pragma once

#include <Arduino.h>
#include <SD.h>
#include <esp_display_panel.hpp>
#include "lvgl_v8_port.h"
#include "usb/helpers/Actions.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

struct ButtonUserData {
    uint8_t index;
    char* iconPath;
};

constexpr uint16_t SCREEN_WIDTH = 800;
constexpr uint16_t SCREEN_HEIGHT = 480;
constexpr uint8_t ROWS = 3;
constexpr uint8_t COLS = 5;
constexpr uint8_t BTN_SIZE = 140;

constexpr uint16_t TOTAL_BUTTON_HEIGHT = ROWS * BTN_SIZE;
constexpr uint16_t SPACING_Y = (SCREEN_HEIGHT - TOTAL_BUTTON_HEIGHT) / (ROWS + 1);
constexpr uint16_t REMAINDER_Y = (SCREEN_HEIGHT - TOTAL_BUTTON_HEIGHT) % (ROWS + 1);
constexpr uint16_t OUTER_MARGIN_Y = REMAINDER_Y / 2;

constexpr uint16_t TOTAL_BUTTON_WIDTH = COLS * BTN_SIZE;
constexpr uint16_t SPACING_X = (SCREEN_WIDTH - TOTAL_BUTTON_WIDTH) / (COLS + 1);
constexpr uint16_t REMAINDER_X = (SCREEN_WIDTH - TOTAL_BUTTON_WIDTH) % (COLS + 1);
constexpr uint16_t OUTER_MARGIN_X = REMAINDER_X / 2;

class Display
{
public:
    static Display *instance;
    using ButtonCallback = std::function<void(uint8_t, lv_event_code_t)>;
    Display(String imageDir);
    ~Display() = default;
    void initializeLVGL(Board &board);
    void ifAvoidTearing(Board &board);
    void renderStartupScreen();
    void renderButtonsFromConfig(const PageConfig &config);
    void setButtonCallback(ButtonCallback cb) { buttonCallback = cb; }

private:
    ButtonCallback buttonCallback = nullptr;
    String imageDir;
    void eventHandler(lv_event_t *e);
    static void eventHandlerWrapper(lv_event_t *e);
};
