#pragma once

#include <Arduino.h>
#include <SD.h>
#include <USB.h>
#include <USBHIDVendor.h>
#include <CRC32.h>
#include "helpers/Commands.h"
#include <span>

class LucydDeckController;

constexpr uint8_t PACKET_SIZE = 64;
constexpr uint8_t CRC_SIZE = 4;
constexpr uint8_t BUFFER_SIZE = PACKET_SIZE - 1;
constexpr uint8_t HEADER_SIZE = 2;
constexpr uint8_t PACKET_DATA_SIZE = BUFFER_SIZE - CRC_SIZE;
constexpr uint8_t PACKET_PAYLOAD_SIZE = PACKET_DATA_SIZE - HEADER_SIZE;

struct UsbMessage
{
    uint8_t data[BUFFER_SIZE];
    size_t len;
};

using Packet = std::span<const uint8_t>;

class CustomHIDDevice
{
public:
    static CustomHIDDevice *instance;
    CustomHIDDevice();
    void begin();
    void loop();
    using PacketCallback = std::function<void(Command, const uint8_t *, size_t)>;
    void setPacketCallback(PacketCallback cb) { packetCallback = cb; }
    bool sendPacket(uint8_t command, const char *data, size_t len);

private:
    PacketCallback packetCallback = nullptr;
    QueueHandle_t usbQueue;
    uint8_t txBuffer[BUFFER_SIZE];
    static void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    void onOutput(uint8_t *buffer, size_t len);
    void handlePacket(Packet packet);
    Packet createPacket(const char *data, size_t len);
    bool sendSinglePacket(uint8_t command, Packet packet);
};
