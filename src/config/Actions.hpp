// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include <vector>
#include <variant>
#include <optional>
#include <map>
#include "keyMappings.hpp"

namespace config
{
    namespace actions
    {
        // Action Data Types
        struct HidKeyAction
        {
            std::vector<uint8_t> keycodes;
        };

        struct ControlKeyAction
        {
            uint16_t keycode; // e.g., "VOLUME_UP", "MUTE"
        };

        struct MouseMoveAction
        {
            int16_t x;
            int16_t y;
            int8_t wheel; // 0 if not used
        };

        struct MouseClickAction
        {
            String button; // e.g., "LEFT", "RIGHT", "MIDDLE"
            uint8_t count; // 1 for single, 2 for double click
        };

        struct DelayAction
        {
            uint32_t ms;
        };

        struct TextAction
        {
            String text;
        };

        struct CmdAction
        {
            String command;
        };

        struct PageAction
        {
            uint8_t targetPage;
        };
        
        struct ProfileAction
        {
            String targetProfile;
        };

        using ActionData = std::variant<
            HidKeyAction,
            ControlKeyAction,
            MouseMoveAction,
            MouseClickAction,
            DelayAction,
            TextAction,
            CmdAction,
            PageAction,
            ProfileAction>;

        struct ActionStep
        {
            ActionData data;
        };

        using ActionSequence = std::vector<ActionStep>;

        inline bool isInternalAction(const ActionData &data)
        {
            return std::holds_alternative<PageAction>(data) || std::holds_alternative<ProfileAction>(data);
        }

        inline std::optional<uint8_t> parsePageTarget(const String &args)
        {
            if (args.isEmpty() || args.length() > 3)
                return std::nullopt;

            for (unsigned int i = 0; i < args.length(); ++i)
            {
                if (args[i] < '0' || args[i] > '9')
                    return std::nullopt;
            }

            long id = args.toInt();
            if (id < 0 || id > 255)
                return std::nullopt;
            return static_cast<uint8_t>(id);
        }
        
        inline std::optional<ActionData> parseActionString(const String &action)
        {
            if (action.isEmpty())
                return std::nullopt;

            int colon = action.indexOf(':');
            String ns = (colon == -1) ? action : action.substring(0, colon);
            String args = (colon == -1) ? String("") : action.substring(colon + 1);
            ns.trim();

            if (ns == "PAGE")
            {
                std::optional<uint8_t> page = parsePageTarget(args);
                if (!page.has_value())
                    return std::nullopt;
                
                return PageAction{*page};
            }
            if (ns == "PROFILE")
            {
                String name = args;
                name.trim();
                if (name.isEmpty())
                    return std::nullopt;
                return ProfileAction{name};
            }
            if (ns == "HID_KEY")
            {
                HidKeyAction act;
                int start = 0;
                while (true)
                {
                    int plus = args.indexOf('+', start);
                    String token = (plus == -1) ? args.substring(start) : args.substring(start, plus);
                    token.trim();
                    if (!token.isEmpty())
                    {
                        uint16_t code = getKeyValue(token);
                        if (code != 0)
                            act.keycodes.push_back(static_cast<uint8_t>(code));
                    }
                    if (plus == -1)
                        break;
                    start = plus + 1;
                }
                if (act.keycodes.empty())
                    return std::nullopt;
                return act;
            }
            if (ns == "CONTROL_KEY")
            {
                String name = args;
                name.trim();
                uint16_t code = getKeyValue(name);
                if (code == 0)
                    return std::nullopt;
                return ControlKeyAction{code};
            }
            if (ns == "MOUSE_MOVE")
            {
                MouseMoveAction act;
                int comma1 = args.indexOf(',');
                String xStr = (comma1 == -1) ? args : args.substring(0, comma1);
                String rest = (comma1 == -1) ? String("") : args.substring(comma1 + 1);
                int comma2 = rest.indexOf(',');
                String yStr = (comma2 == -1) ? rest : rest.substring(0, comma2);
                String wStr = (comma2 == -1) ? String("") : rest.substring(comma2 + 1);
                act.x = static_cast<int16_t>(xStr.toInt());
                act.y = static_cast<int16_t>(yStr.toInt());
                act.wheel = static_cast<int8_t>(wStr.toInt());
                return act;
            }
            if (ns == "MOUSE_CLICK")
            {
                MouseClickAction act;
                int comma = args.indexOf(',');
                String btnStr = (comma == -1) ? args : args.substring(0, comma);
                String countStr = (comma == -1) ? String("") : args.substring(comma + 1);
                btnStr.trim();
                if (btnStr != "LEFT" && btnStr != "RIGHT" && btnStr != "MIDDLE")
                    return std::nullopt;
                act.button = btnStr;
                int count = countStr.isEmpty() ? 1 : countStr.toInt();
                if (count < 1)
                    count = 1;
                if (count > 10)
                    count = 10;
                act.count = static_cast<uint8_t>(count);
                return act;
            }
            if (ns == "DELAY")
            {
                DelayAction act;
                long ms = args.toInt();
                act.ms = (ms > 0) ? static_cast<uint32_t>(ms) : 0;
                return act;
            }
            if (ns == "TEXT")
            {
                return TextAction{args};
            }
            if (ns == "CMD")
            {
                return CmdAction{args};
            }

            return std::nullopt;
        }
        
        struct ButtonConfig
        {
            String imageName;
            String label;
            String backgroundColor;
            ActionSequence click;
            ActionSequence longPress;
        };
        using PageConfig = std::map<uint8_t, ButtonConfig>;
        
        using PageList = std::map<uint8_t, String>;
        using ProfileList = std::map<String, PageList>;
    }
}
