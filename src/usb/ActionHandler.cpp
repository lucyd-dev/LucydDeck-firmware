#include "ActionHandler.h"

ActionHandler::ActionHandler(
    USBHIDKeyboard &keyboard,
    USBHIDConsumerControl &consumer,
    USBHIDMouse &mouse) : keyboard(keyboard), consumer(consumer), mouse(mouse) {}

void ActionHandler::execute(const ActionStep &step)
{
    std::visit([this](auto &&arg)
               {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, HidKeyAction>) {
            executeHidKey(arg);
        } else if constexpr (std::is_same_v<T, ControlKeyAction>) {
            executeControlKey(arg);
        } else if constexpr (std::is_same_v<T, MouseMoveAction>) {
            executeMouseMove(arg);
        } else if constexpr (std::is_same_v<T, MouseClickAction>) {
            executeMouseClick(arg);
        } else if constexpr (std::is_same_v<T, DelayAction>) {
            executeDelay(arg);
        } else if constexpr (std::is_same_v<T, TextAction>) {
            executeText(arg);
        } else if constexpr (std::is_same_v<T, CmdAction>) {
            executeCmd(arg);
        } else if constexpr (std::is_same_v<T, PageAction>) {
            executePage(arg);
        } }, step.data);
}

void ActionHandler::executeSequence(const ActionSequence &sequence)
{
    for (const auto &step : sequence)
    {
        execute(step);
    }
}

void ActionHandler::executeHidKey(const HidKeyAction &action)
{
    for (uint8_t code : action.keycodes)
    {
        Serial0.printf("Executing hid key: %d\n", code);
        keyboard.press(code);
    }
    delay(10);
    keyboard.releaseAll();
}

void ActionHandler::executeControlKey(const ControlKeyAction &action)
{
    if (action.keycode != 0)
    {
        consumer.press(action.keycode);
        delay(10);
        consumer.release();
    }
}

void ActionHandler::executeMouseMove(const MouseMoveAction &action)
{
    mouse.move(action.x, action.y, action.wheel);
}

void ActionHandler::executeMouseClick(const MouseClickAction &action)
{
    uint8_t btn = 0;
    if (action.button == "LEFT")
        btn = MOUSE_LEFT;
    else if (action.button == "RIGHT")
        btn = MOUSE_RIGHT;
    else if (action.button == "MIDDLE")
        btn = MOUSE_MIDDLE;
    for (uint8_t i = 0; i < action.count; ++i)
    {
        mouse.press(btn);
        delay(10);
        mouse.release(btn);
        delay(50);
    }
}

void ActionHandler::executeDelay(const DelayAction &action)
{
    delay(action.ms);
}

void ActionHandler::executeText(const TextAction &action)
{
    keyboard.print(action.text);
}

void ActionHandler::executeCmd(const CmdAction &action)
{
    // This should be handled by LucydDeckController, not here.
    // You can leave this empty or call a callback if you want.
}

void ActionHandler::executePage(const PageAction &action)
{
    // This should be handled by LucydDeckController, not here.
    // You can leave this empty or call a callback if you want.
}

std::vector<uint16_t> ActionHandler::getKeyCodes(const std::vector<String> &keys)
{
    std::vector<uint16_t> codes;
    for (const auto &key : keys)
    {
        uint16_t code = getKeyValue(key.c_str());
        if (code != 0)
            codes.push_back(code);
    }
    return codes;
}
