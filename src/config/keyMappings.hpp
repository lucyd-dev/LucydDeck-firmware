// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"

namespace config
{

    struct KeyMap
    {
        const char *name;
        uint16_t value;
    };

    // Consumer Control key mappings
    inline const KeyMap consumerControlKeys[] = {
        {"BACK", CONSUMER_CONTROL_BACK},
        {"BOOKMARKS", CONSUMER_CONTROL_BOOKMARKS},
        {"CALCULATOR", CONSUMER_CONTROL_CALCULATOR},
        {"EMAIL_READER", CONSUMER_CONTROL_EMAIL_READER},
        {"FORWARD", CONSUMER_CONTROL_FORWARD},
        {"HOME", CONSUMER_CONTROL_HOME},
        {"LOCAL_BROWSER", CONSUMER_CONTROL_LOCAL_BROWSER},
        {"MUTE", CONSUMER_CONTROL_MUTE},
        {"PLAY_PAUSE", CONSUMER_CONTROL_PLAY_PAUSE},
        {"REFRESH", CONSUMER_CONTROL_REFRESH},
        {"NEXT", CONSUMER_CONTROL_SCAN_NEXT},
        {"PREVIOUS", CONSUMER_CONTROL_SCAN_PREVIOUS},
        {"SEARCH", CONSUMER_CONTROL_SEARCH},
        {"STOP", CONSUMER_CONTROL_STOP},
        {"VOLUME_DOWN", CONSUMER_CONTROL_VOLUME_DECREMENT},
        {"VOLUME_UP", CONSUMER_CONTROL_VOLUME_INCREMENT}};
    inline const size_t consumerControlKeysCount = sizeof(consumerControlKeys) / sizeof(consumerControlKeys[0]);

    // Special keyboard key mappings (F1-F12, arrows, etc.)
    inline const KeyMap specialKeys[] = {
        // Modifier keys
        {"CTRL", KEY_LEFT_CTRL},
        {"SHIFT", KEY_LEFT_SHIFT},
        {"ALT", KEY_LEFT_ALT},
        {"GUI", KEY_LEFT_GUI},
        {"RIGHT_CTRL", KEY_RIGHT_CTRL},
        {"RIGHT_SHIFT", KEY_RIGHT_SHIFT},
        {"RIGHT_ALT", KEY_RIGHT_ALT},
        {"RIGHT_GUI", KEY_RIGHT_GUI},

        // Arrow keys
        {"UP", KEY_UP_ARROW},
        {"DOWN", KEY_DOWN_ARROW},
        {"LEFT", KEY_LEFT_ARROW},
        {"RIGHT", KEY_RIGHT_ARROW},

        // Special keys
        {"BACKSPACE", KEY_BACKSPACE},
        {"TAB", KEY_TAB},
        {"ENTER", KEY_RETURN},
        {"ESC", KEY_ESC},
        {"INSERT", KEY_INSERT},
        {"DELETE", KEY_DELETE},
        {"PAGE_UP", KEY_PAGE_UP},
        {"PAGE_DOWN", KEY_PAGE_DOWN},
        {"HOME", KEY_HOME},
        {"END", KEY_END},
        {"CAPS_LOCK", KEY_CAPS_LOCK},
        {"NUM_LOCK", KEY_NUM_LOCK},
        {"PRINT_SCREEN", KEY_PRINT_SCREEN},
        {"SCROLL_LOCK", KEY_SCROLL_LOCK},
        {"PAUSE", KEY_PAUSE},
        {"SPACE", ' '},

        // Function keys
        {"F1", KEY_F1},
        {"F2", KEY_F2},
        {"F3", KEY_F3},
        {"F4", KEY_F4},
        {"F5", KEY_F5},
        {"F6", KEY_F6},
        {"F7", KEY_F7},
        {"F8", KEY_F8},
        {"F9", KEY_F9},
        {"F10", KEY_F10},
        {"F11", KEY_F11},
        {"F12", KEY_F12},
        {"F13", KEY_F13},
        {"F14", KEY_F14},
        {"F15", KEY_F15},
        {"F16", KEY_F16},
        {"F17", KEY_F17},
        {"F18", KEY_F18},
        {"F19", KEY_F19},
        {"F20", KEY_F20},
        {"F21", KEY_F21},
        {"F22", KEY_F22},
        {"F23", KEY_F23},
        {"F24", KEY_F24},

        // Numeric keypad
        {"KP_SLASH", KEY_KP_SLASH},
        {"KP_ASTERISK", KEY_KP_ASTERISK},
        {"KP_MINUS", KEY_KP_MINUS},
        {"KP_PLUS", KEY_KP_PLUS},
        {"KP_ENTER", KEY_KP_ENTER},
        {"KP_1", KEY_KP_1},
        {"KP_2", KEY_KP_2},
        {"KP_3", KEY_KP_3},
        {"KP_4", KEY_KP_4},
        {"KP_5", KEY_KP_5},
        {"KP_6", KEY_KP_6},
        {"KP_7", KEY_KP_7},
        {"KP_8", KEY_KP_8},
        {"KP_9", KEY_KP_9},
        {"KP_0", KEY_KP_0},
        {"KP_DOT", KEY_KP_DOT},

    };
    inline const size_t specialKeysCount = sizeof(specialKeys) / sizeof(specialKeys[0]);

    inline static uint16_t linearSearchKey(const KeyMap *keys, size_t count, const String &keyName)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (keyName.equals(keys[i].name))
                return keys[i].value;
        }
        return 0;
    }

    inline uint16_t getKeyValue(const String &keyName)
    {
        if (keyName.length() == 1)
            return keyName[0];
        uint16_t value = linearSearchKey(specialKeys, specialKeysCount, keyName);
        if (value != 0)
            return value;
        return linearSearchKey(consumerControlKeys, consumerControlKeysCount, keyName);
    }

}
