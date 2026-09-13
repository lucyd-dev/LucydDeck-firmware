// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "UsbManager.hpp"
#include <variant>

namespace
{
    // Helper for std::visit to create a visitor from lambdas
    template <class... Ts>
    struct overloaded : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;
}

namespace usb
{
    using namespace config::actions;
    UsbManager *UsbManager::instance = nullptr;

    UsbManager::UsbManager() : keyboard(), consumer(), mouse(), customHIDDevice()
    {
        instance = this;
        actionQueue = xQueueCreate(ACTION_QUEUE_LENGTH, sizeof(ActionSequence *));
        if (actionQueue == nullptr)
        {
            Serial0.println("[ERR] Failed to create action queue!");
        }
        internalQueue = xQueueCreate(INTERNAL_QUEUE_LENGTH, sizeof(ActionData *));
        if (internalQueue == nullptr)
        {
            Serial0.println("[ERR] Failed to create internal action queue!");
        }
    }

    UsbManager::~UsbManager()
    {
        if (actionTaskHandle != nullptr)
        {
            vTaskDelete(actionTaskHandle);
            actionTaskHandle = nullptr;
        }
        if (actionQueue)
        {
            vQueueDelete(actionQueue);
            actionQueue = nullptr;
        }
        if (internalQueue)
        {
            vQueueDelete(internalQueue);
            internalQueue = nullptr;
        }
        instance = nullptr;
    }

    void UsbManager::begin()
    {
        Serial.onEvent(SerialEventCallback);
        USB.onEvent(usbEventCallback);

        Serial.begin();
        keyboard.begin();
        consumer.begin();
        mouse.begin();
        customHIDDevice.begin();
        USB.begin();

#ifdef ARDUINO_RUNNING_CORE
        BaseType_t ret = xTaskCreatePinnedToCore(actionWorker, "actions", ACTION_TASK_STACK_SIZE, this,
                                                 ACTION_TASK_PRIORITY, &actionTaskHandle, ARDUINO_RUNNING_CORE);
#else
        BaseType_t ret = xTaskCreate(actionWorker, "actions", ACTION_TASK_STACK_SIZE, this,
                                     ACTION_TASK_PRIORITY, &actionTaskHandle);
#endif
        if (ret != pdPASS)
        {
            Serial0.println("[ERR] Failed to create action task!");
            actionTaskHandle = nullptr;
        }
    }

    void UsbManager::loop()
    {
        customHIDDevice.loop();
    }

    void UsbManager::SerialEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        if (event_base != ARDUINO_USB_CDC_EVENTS)
            return;

        switch (event_id)
        {
        case ARDUINO_USB_CDC_CONNECTED_EVENT:
            Serial0.println("USB CDC Connected");
            break;
        case ARDUINO_USB_CDC_DISCONNECTED_EVENT:
            Serial0.println("USB CDC Disconnected");
            break;
        default:
            break;
        }
    }

    void UsbManager::usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        if (event_base != ARDUINO_USB_EVENTS)
            return;

        arduino_usb_event_data_t *data = reinterpret_cast<arduino_usb_event_data_t *>(event_data);
        switch (event_id)
        {
        case ARDUINO_USB_STARTED_EVENT:
        case ARDUINO_USB_RESUME_EVENT:
            instance->usbConnected = true;
            instance->customHIDDevice.resetSequence();
            Serial0.println("USB Connected");
            break;
        case ARDUINO_USB_STOPPED_EVENT:
        case ARDUINO_USB_SUSPEND_EVENT:
            instance->usbConnected = false;
            Serial0.println("USB Disconnected");
            break;
        default:
            break;
        }
    }

    bool UsbManager::postSequence(const ActionSequence &sequence)
    {
        if (actionQueue == nullptr)
            return false;

        ActionSequence *copy = new ActionSequence(sequence);
        if (xQueueSend(actionQueue, &copy, 0) != pdTRUE)
        {
            Serial0.println("[WARN] Action queue full, dropping sequence");
            delete copy;
            return false;
        }
        return true;
    }

    bool UsbManager::postInternal(const ActionData &action)
    {
        if (internalQueue == nullptr)
            return false;

        ActionData *copy = new ActionData(action);
        if (xQueueSend(internalQueue, &copy, 0) != pdTRUE)
        {
            Serial0.println("[WARN] Internal action queue full, dropping action");
            delete copy;
            return false;
        }
        return true;
    }

    bool UsbManager::takeInternal(ActionData &out)
    {
        if (internalQueue == nullptr)
            return false;

        ActionData *item = nullptr;
        if (xQueueReceive(internalQueue, &item, 0) == pdTRUE && item != nullptr)
        {
            out = std::move(*item);
            delete item;
            return true;
        }
        return false;
    }

    void UsbManager::actionWorker(void *arg)
    {
        UsbManager *self = static_cast<UsbManager *>(arg);
        while (true)
        {
            ActionSequence *seq = nullptr;
            if (xQueueReceive(self->actionQueue, &seq, portMAX_DELAY) == pdTRUE && seq != nullptr)
            {
                for (const auto &step : *seq)
                {
                    self->execute(step);
                }
                delete seq;
            }
        }
    }

    void UsbManager::execute(const ActionStep &step)
    {
#if CORE_DEBUG_LEVEL >= 4
        Serial0.printf("[ACT] type=%u\n", (unsigned)step.data.index());
#endif

        if (isInternalAction(step.data))
        {
            postInternal(step.data);
            return;
        }

        if (!usbConnected)
        {
#if CORE_DEBUG_LEVEL >= 4
            Serial0.println("[ACT] dropped (not connected)");
#endif
            return;
        }

        std::visit(
            overloaded{
                [this](const HidKeyAction &a)
                { executeHidKey(a); },
                [this](const ControlKeyAction &a)
                { executeControlKey(a); },
                [this](const MouseMoveAction &a)
                { executeMouseMove(a); },
                [this](const MouseClickAction &a)
                { executeMouseClick(a); },
                [this](const DelayAction &a)
                { executeDelay(a); },
                [this](const TextAction &a)
                { executeText(a); },
                [this](const CmdAction &a)
                { executeCmd(a); },
                [](const auto &) {}},
            step.data);
    }

    void UsbManager::executeHidKey(const HidKeyAction &action)
    {
        for (uint8_t code : action.keycodes)
        {
            keyboard.press(code);
        }
        delay(10);
        keyboard.releaseAll();
    }

    void UsbManager::executeControlKey(const ControlKeyAction &action)
    {
        if (action.keycode != 0)
        {
            consumer.press(action.keycode);
            delay(10);
            consumer.release();
        }
    }

    void UsbManager::executeMouseMove(const MouseMoveAction &action)
    {
        mouse.move(action.x, action.y, action.wheel);
    }

    void UsbManager::executeMouseClick(const MouseClickAction &action)
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

    void UsbManager::executeDelay(const DelayAction &action)
    {
        delay(action.ms);
    }

    void UsbManager::executeText(const TextAction &action)
    {
        keyboard.print(action.text);
    }

    void UsbManager::executeCmd(const CmdAction &action)
    {
        String command = action.command;
        if (command.startsWith(F("CMD:")))
            command = command.substring(4);

#if CORE_DEBUG_LEVEL >= 4
        Serial0.printf("[ACT] cmd='%s'\n", command.c_str());
#endif
        sendPacket(EVT_ACTION_TRIGGERED, command);
    }
}
