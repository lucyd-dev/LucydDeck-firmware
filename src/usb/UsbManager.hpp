// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <functional>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDConsumerControl.h>
#include <USBHIDMouse.h>
#include "CustomHIDDevice.hpp"
#include "config/Actions.hpp"

namespace usb
{
    class UsbManager
    {
    public:
        UsbManager();
        ~UsbManager();
        void begin();
        void loop();
        using PacketCallback = CustomHIDDevice::PacketCallback;
        void setPacketCallback(const PacketCallback &cb) { customHIDDevice.setPacketCallback(cb); }
        bool sendPacket(OpCode opCode, String payload) { return customHIDDevice.sendPacket(opCode, payload.c_str(), payload.length()); }
        void sendAck() { customHIDDevice.sendAck(); }
        void sendError(const ErrorCode error) { customHIDDevice.sendError(error); }
        bool isConnected() const { return usbConnected; }
        bool postSequence(const config::actions::ActionSequence &sequence);
        bool postInternal(const config::actions::ActionData &action);
        bool takeInternal(config::actions::ActionData &out);
        void execute(const config::actions::ActionStep &step);

    private:
        static UsbManager *instance;
        static constexpr uint8_t ACTION_QUEUE_LENGTH = 8;
        static constexpr uint8_t INTERNAL_QUEUE_LENGTH = 4;
        static constexpr uint16_t ACTION_TASK_STACK_SIZE = 6 * 1024;
        static constexpr uint8_t ACTION_TASK_PRIORITY = 1;
        USBHIDKeyboard keyboard;
        USBHIDConsumerControl consumer;
        USBHIDMouse mouse;
        CustomHIDDevice customHIDDevice;
        QueueHandle_t actionQueue;
        QueueHandle_t internalQueue;
        TaskHandle_t actionTaskHandle = nullptr;
        bool usbConnected = false;

        static void SerialEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
        static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
        static void actionWorker(void *arg);
        void executeHidKey(const config::actions::HidKeyAction &action);
        void executeControlKey(const config::actions::ControlKeyAction &action);
        void executeMouseMove(const config::actions::MouseMoveAction &action);
        void executeMouseClick(const config::actions::MouseClickAction &action);
        void executeDelay(const config::actions::DelayAction &action);
        void executeText(const config::actions::TextAction &action);
        void executeCmd(const config::actions::CmdAction &action);
    };
}
