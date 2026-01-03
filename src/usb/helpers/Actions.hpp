#pragma once

#include <Arduino.h>
#include <vector>
#include <variant>
#include <map>

// --- Action Type Enum ---

enum class ActionTypeEnum {
    HID_KEY,
    CONTROL_KEY,
    MOUSE_MOVE,
    MOUSE_CLICK,
    DELAY,
    TEXT,
    CMD,
    PAGE,
    UNKNOWN
};

// --- Action Data Types ---

struct HidKeyAction {
    std::vector<uint8_t> keycodes;
};

struct ControlKeyAction {
    uint16_t keycode; // e.g., "VOLUME_UP", "MUTE"
};

struct MouseMoveAction {
    int16_t x;
    int16_t y;
    int8_t wheel; // 0 if not used
};

struct MouseClickAction {
    String button; // e.g., "LEFT", "RIGHT", "MIDDLE"
    uint8_t count; // 1 for single, 2 for double click
};

struct DelayAction {
    uint32_t ms;
};

struct TextAction {
    String text;
};

struct CmdAction {
    String command;
};

struct PageAction {
    uint8_t targetPage;
};

// --- Action Variant ---

using ActionData = std::variant<
    HidKeyAction,
    ControlKeyAction,
    MouseMoveAction,
    MouseClickAction,
    DelayAction,
    TextAction,
    CmdAction,
    PageAction
>;

struct ActionStep {
    ActionData data;
};

using ActionSequence = std::vector<ActionStep>;

inline ActionTypeEnum actionTypeFromString(const String& s) {
    if (s == "HID_KEY") return ActionTypeEnum::HID_KEY;
    if (s == "CONTROL_KEY") return ActionTypeEnum::CONTROL_KEY;
    if (s == "MOUSE_MOVE") return ActionTypeEnum::MOUSE_MOVE;
    if (s == "MOUSE_CLICK") return ActionTypeEnum::MOUSE_CLICK;
    if (s == "DELAY") return ActionTypeEnum::DELAY;
    if (s == "TEXT") return ActionTypeEnum::TEXT;
    if (s == "CMD") return ActionTypeEnum::CMD;
    if (s == "PAGE") return ActionTypeEnum::PAGE;
    return ActionTypeEnum::UNKNOWN;
}

// --- Button Config ---

using ButtonId = int;
struct ButtonConfig {
    String imageName;
    ActionSequence click;
    ActionSequence longPress;
};
using PageConfig = std::map<ButtonId, ButtonConfig>;
