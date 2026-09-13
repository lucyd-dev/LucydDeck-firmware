// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include <functional>
#include <span>
#include "OpCodes.hpp"
#include "Errors.hpp"

namespace usb
{
    constexpr uint8_t BUFFER_SIZE = 63;
    constexpr uint8_t HEADER_SIZE = 3;
    constexpr uint8_t PACKET_PAYLOAD_SIZE = BUFFER_SIZE - HEADER_SIZE;
    constexpr uint32_t TX_LOCK_TIMEOUT_MS = 1000;

    struct Packet
    {
        uint8_t length;
        uint8_t data[BUFFER_SIZE];

        Packet() : length(0) { memset(data, 0, BUFFER_SIZE); }
        Packet(const uint8_t *src, uint8_t len)
        {
            if (len > BUFFER_SIZE)
                len = BUFFER_SIZE;
            length = len;
            memset(data, 0, BUFFER_SIZE);
            memcpy(data, src, len);
        }
    };

    class CustomHIDDevice
    {
    public:
        CustomHIDDevice();
        ~CustomHIDDevice();
        void begin();
        void loop();
        using PacketCallback = std::function<void(OpCode, const uint16_t sequence, const uint8_t *data, size_t len)>;
        void setPacketCallback(PacketCallback cb) { packetCallback = cb; }
        bool sendPacket(OpCode opCode, const char *data, size_t len);
        void sendAck();
        void sendError(ErrorCode error);
        void resetSequence() { lastSequence = 0; sequenceInitialized = false; }

    private:
        static CustomHIDDevice *instance;
        PacketCallback packetCallback = nullptr;
        QueueHandle_t usbQueue;
        SemaphoreHandle_t txMutex;
        uint8_t txBuffer[BUFFER_SIZE];
        uint16_t lastSequence = 0;
        bool sequenceInitialized = false;

        static void vendorEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
        void onOutput(const Packet &packet);
        void handlePacket(const Packet &packet);
        bool sendSinglePacket(OpCode command, uint16_t sequence, const char *data, size_t len);
    };
}
