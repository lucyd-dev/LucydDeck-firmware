#include "CustomHIDDevice.h"

USBHIDVendor Vendor(PACKET_SIZE, false);
CustomHIDDevice *CustomHIDDevice::instance = nullptr;

CustomHIDDevice::CustomHIDDevice()
{
    instance = this;
    usbQueue = xQueueCreate(10, sizeof(UsbMessage));
    if (usbQueue == nullptr)
    {
        Serial0.println("Failed to create USB queue!");
    }
}

void CustomHIDDevice::begin()
{
    USB.onEvent(usbEventCallback);
    Vendor.onEvent(usbEventCallback);
    Vendor.begin();
    USB.begin();
}

void CustomHIDDevice::loop()
{
    if (!usbQueue)
        return;

    UsbMessage msg;
    if (xQueueReceive(usbQueue, &msg, 0) == pdTRUE)
    {
        handlePacket(msg.data);
    }
}

void CustomHIDDevice::usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == ARDUINO_USB_EVENTS)
    {
        arduino_usb_event_data_t *data = (arduino_usb_event_data_t *)event_data;
        switch (event_id)
        {
        case ARDUINO_USB_STARTED_EVENT:
            Serial0.println("USB PLUGGED");
            break;
        case ARDUINO_USB_STOPPED_EVENT:
            Serial0.println("USB UNPLUGGED");
            break;
        case ARDUINO_USB_SUSPEND_EVENT:
            Serial0.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en);
            break;
        case ARDUINO_USB_RESUME_EVENT:
            Serial0.println("USB RESUMED");
            break;

        default:
            break;
        }
    }
    else if (event_base == ARDUINO_USB_HID_VENDOR_EVENTS)
    {
        if (event_id != ARDUINO_USB_HID_VENDOR_OUTPUT_EVENT)
            return;

        arduino_usb_hid_vendor_event_data_t *data = (arduino_usb_hid_vendor_event_data_t *)event_data;
        uint8_t buffer[BUFFER_SIZE];
        Vendor.read(buffer, data->len);
        if (instance)
            instance->onOutput(buffer, data->len);
    }
}

void CustomHIDDevice::onOutput(uint8_t *buffer, size_t len)
{
    UsbMessage msg;
    memcpy(msg.data, buffer, len);
    msg.len = len;
    if (!usbQueue || xQueueSend(usbQueue, &msg, 0) != pdTRUE)
    {
        Serial0.println("[ERR] USB queue full or not ready");
    }
}

void CustomHIDDevice::handlePacket(Packet packet)
{
    if (packet.size() < 6)
        return;

    auto command = static_cast<Command>(packet[0]);
    auto dataLen = static_cast<size_t>(packet[1]);
    auto payload = packet.subspan(2, dataLen);
    auto receivedCrc = *reinterpret_cast<const uint32_t *>(packet.data() + PACKET_DATA_SIZE);
    auto calculatedCrc = CRC32::calculate(packet.data(), PACKET_DATA_SIZE);

    if (receivedCrc != calculatedCrc)
    {
        Serial0.printf("CRC mismatch, got %08X, expected %08X\n", receivedCrc, calculatedCrc);
        sendPacket(CMD_ERROR, "crc_error", strlen("crc_error"));
        return;
    }

    if (packetCallback)
        packetCallback(command, payload.data(), payload.size());
}

Packet CustomHIDDevice::createPacket(const char *data, size_t len)
{
    uint8_t buffer[PACKET_PAYLOAD_SIZE];
    memset(buffer, 0, PACKET_PAYLOAD_SIZE);
    memcpy(buffer, data, len);
    return Packet(buffer);
}

bool CustomHIDDevice::sendPacket(uint8_t command, const char *data, size_t len)
{
    Packet packet = createPacket(data, len);

    // Serial0.printf("Sending packet: 0x%02X (len:%d)\n", command, len);
    if (packet.size() > PACKET_PAYLOAD_SIZE)
    {
        size_t remainingLen = packet.size();
        size_t offset = 0;
        bool success = true;

        while (remainingLen > 0)
        {
            size_t packetLen = min((size_t)PACKET_PAYLOAD_SIZE, remainingLen);

            if (!sendSinglePacket(command, packet.subspan(offset, packetLen)))
            {
                Serial0.printf("[ERR] Failed to send chunked packet (len:%d)\n", packetLen);
                return false;
            }
            remainingLen -= packetLen;
            offset += packetLen;
        }
        return true;
    }

    return sendSinglePacket(command, packet);
}

bool CustomHIDDevice::sendSinglePacket(uint8_t command, Packet packet)
{
    static bool in_send = false;
    if (in_send)
    {
        Serial0.println("[ERR] Recursive send detected!");
        return false;
    }
    in_send = true;

    if (packet.size() > PACKET_PAYLOAD_SIZE)
    {
        Serial0.println("[ERR] Packet too large for sendSinglePacket");
        in_send = false;
        return false;
    }

    memset(txBuffer, 0, BUFFER_SIZE);
    txBuffer[0] = command;
    txBuffer[1] = packet.size();

    memcpy(txBuffer + 1, packet.data(), packet.size());

    uint32_t crc = CRC32::calculate(txBuffer, PACKET_DATA_SIZE);
    memcpy(txBuffer + PACKET_DATA_SIZE, &crc, CRC_SIZE);

    size_t bytesWritten = Vendor.write(txBuffer, BUFFER_SIZE);
    in_send = false;

    return bytesWritten == BUFFER_SIZE;
}
