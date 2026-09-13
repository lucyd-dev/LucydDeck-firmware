// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "CustomHIDDevice.hpp"
#include <USB.h>
#include <USBHIDVendor.h>

namespace usb
{
    USBHIDVendor Vendor(BUFFER_SIZE, false);
    CustomHIDDevice *CustomHIDDevice::instance = nullptr;

    CustomHIDDevice::CustomHIDDevice()
    {
        instance = this;
        usbQueue = xQueueCreate(10, sizeof(Packet));
        if (usbQueue == nullptr)
        {
            Serial0.println("[ERR] Failed to create USB queue!");
        }
        txMutex = xSemaphoreCreateRecursiveMutex();
        if (txMutex == nullptr)
        {
            Serial0.println("[ERR] Failed to create USB TX mutex!");
        }
    }

    CustomHIDDevice::~CustomHIDDevice()
    {
        instance = nullptr;
        if (usbQueue)
        {
            vQueueDelete(usbQueue);
            usbQueue = nullptr;
        }
        if (txMutex)
        {
            vSemaphoreDelete(txMutex);
            txMutex = nullptr;
        }
    }

    void CustomHIDDevice::begin()
    {
        Vendor.onEvent(vendorEventCallback);

        Vendor.begin();
    }

    void CustomHIDDevice::loop()
    {
        if (!usbQueue)
            return;

        Packet packet;
        if (xQueueReceive(usbQueue, &packet, 0) == pdTRUE)
        {
            handlePacket(packet);
        }
    }

    void CustomHIDDevice::vendorEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
    {
        if (event_base != ARDUINO_USB_HID_VENDOR_EVENTS || event_id != ARDUINO_USB_HID_VENDOR_OUTPUT_EVENT)
            return;

        if (instance == nullptr)
            return;

        arduino_usb_hid_vendor_event_data_t *data = reinterpret_cast<arduino_usb_hid_vendor_event_data_t *>(event_data);
        uint8_t buffer[BUFFER_SIZE];
        size_t len = Vendor.read(buffer, data->len);
        if (len == static_cast<size_t>(-1))
            len = 0;
        instance->onOutput(Packet(buffer, static_cast<uint8_t>(len)));
    }

    void CustomHIDDevice::onOutput(const Packet &packet)
    {
        if (!usbQueue)
            return;

        if (xQueueSend(usbQueue, &packet, 0) != pdTRUE)
            Serial0.println("[WARN] USB RX queue full, dropping packet");
    }

    void CustomHIDDevice::handlePacket(const Packet &packet)
    {
        if (packet.length < HEADER_SIZE)
            return;

        auto opCode = static_cast<OpCode>(packet.data[0]);
        uint16_t sequence = packet.data[1] << 8 | packet.data[2];
        const uint8_t *payloadPtr = packet.data + 3;

        // Host frames must use host-direction opcodes (bit 7 clear).
        if (packet.data[0] & 0x80)
        {
            sendError(ErrorCode::ERR_INVALID_DIRECTION);
            return;
        }

        // Host frames must have M=0; the M-bit is reserved for device→host streams.
        if ((sequence & 0x8000) != 0)
        {
            sendError(ErrorCode::ERR_SEQUENCE);
            return;
        }

        // Strict last+1 within the 15-bit host sequence space once the first
        // frame has established the baseline. Masked compare lets the counter
        // wrap through 0x7FFF → 0 instead of wedging on the M-bit collision.
        if (sequenceInitialized && (sequence & 0x7FFF) != ((lastSequence + 1) & 0x7FFF))
        {
            Serial0.printf("[ERR] Sequence mismatch, got %d, expected %d\n", sequence & 0x7FFF, (lastSequence + 1) & 0x7FFF);
            sendError(ErrorCode::ERR_SEQUENCE);
            return;
        }
        lastSequence = sequence & 0x7FFF;
        sequenceInitialized = true;

        if (packetCallback)
            packetCallback(opCode, sequence, payloadPtr, packet.length - HEADER_SIZE);
    }

    bool CustomHIDDevice::sendPacket(OpCode opCode, const char *data, size_t len)
    {
        if (txMutex == nullptr || xSemaphoreTakeRecursive(txMutex, TX_LOCK_TIMEOUT_MS) != pdTRUE)
        {
            Serial0.println("[ERR] TX lock timeout, dropping send");
            return false;
        }

        size_t remainingLen = len;
        size_t offset = 0;
        uint16_t chunkIndex = 0;
        uint16_t totalChunks = (len == 0) ? 1 : static_cast<uint16_t>((len + PACKET_PAYLOAD_SIZE - 1) / PACKET_PAYLOAD_SIZE);

        do
        {
            size_t chunkLen = (remainingLen > PACKET_PAYLOAD_SIZE) ? PACKET_PAYLOAD_SIZE : remainingLen;
            const char *chunkPtr = (data != nullptr) ? data + offset : nullptr;

            // Device→host stream: chunk k of n → seq = k | (k < n-1 ? 0x8000 : 0).
            // M-bit (0x8000) set while more frames follow, clear on the final frame.
            uint16_t sequence = chunkIndex;
            if (chunkIndex < totalChunks - 1)
                sequence |= 0x8000;

            if (!sendSinglePacket(opCode, sequence, chunkPtr, chunkLen))
            {
                Serial0.printf("[ERR] Failed to send chunk (len:%d)\n", chunkLen);
                xSemaphoreGiveRecursive(txMutex);
                return false;
            }
            chunkIndex++;
            remainingLen -= chunkLen;
            offset += chunkLen;
        } while (remainingLen > 0 || chunkIndex < totalChunks);

        xSemaphoreGiveRecursive(txMutex);
        return true;
    }

    bool CustomHIDDevice::sendSinglePacket(OpCode opCode, uint16_t sequence, const char *data, size_t len)
    {
        if (txMutex == nullptr || xSemaphoreTakeRecursive(txMutex, TX_LOCK_TIMEOUT_MS) != pdTRUE)
        {
            Serial0.println("[ERR] TX lock timeout, dropping packet");
            return false;
        }

        if (len > PACKET_PAYLOAD_SIZE)
        {
            Serial0.println("[ERR] Packet too large for sendSinglePacket");
            xSemaphoreGiveRecursive(txMutex);
            return false;
        }

        memset(txBuffer, 0, BUFFER_SIZE);
        txBuffer[0] = opCode;                // Byte 0: Command
        txBuffer[1] = (sequence >> 8) & 0xFF; // Byte 1: Sequence High
        txBuffer[2] = sequence & 0xFF;

        if (len > 0 && data != nullptr)
        {
            memcpy(txBuffer + HEADER_SIZE, data, len);
        }

        size_t bytesWritten = Vendor.write(txBuffer, BUFFER_SIZE);
        xSemaphoreGiveRecursive(txMutex);

        return bytesWritten == BUFFER_SIZE;
    }

    void CustomHIDDevice::sendAck()
    {
        sendPacket(RESP_ACK, nullptr, 0);
    }

    void CustomHIDDevice::sendError(ErrorCode error)
    {
        uint8_t payload[1];
        payload[0] = static_cast<uint8_t>(error);
        sendPacket(RESP_ERROR, reinterpret_cast<const char *>(payload), 1);
    }
}
