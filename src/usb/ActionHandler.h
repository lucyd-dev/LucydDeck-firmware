#pragma once

#include <Arduino.h>
#include <USBHIDKeyboard.h>
#include <USBHIDConsumerControl.h>
#include <USBHIDMouse.h>
#include "helpers/Actions.h"
#include "helpers/keyMappings.h"

class ActionHandler
{
public:
    ActionHandler(USBHIDKeyboard &keyboard, USBHIDConsumerControl &consumer, USBHIDMouse &mouse);

    void execute(const ActionStep &step);
    void executeSequence(const ActionSequence &sequence);

private:
    USBHIDKeyboard &keyboard;
    USBHIDConsumerControl &consumer;
    USBHIDMouse &mouse;

    void executeHidKey(const HidKeyAction &action);
    void executeControlKey(const ControlKeyAction &action);
    void executeMouseMove(const MouseMoveAction &action);
    void executeMouseClick(const MouseClickAction &action);
    void executeDelay(const DelayAction &action);
    void executeText(const TextAction &action);
    void executeCmd(const CmdAction &action);
    void executePage(const PageAction &action);

    // Helper: Convert key names to keycodes
    std::vector<uint16_t> getKeyCodes(const std::vector<String> &keys);
};
