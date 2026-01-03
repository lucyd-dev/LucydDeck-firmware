#pragma once

#include <RotaryEncOverMCP.h>
#include <map>
#include <vector>
#include <tuple>
#include "hardware/keypad/keypadHandler.h"
#include "network/packetProcessor.h"

enum class RotaryEvent : uint8_t
{
    RotationClockwise = 0,
    RotationCounterClockwise = 1,
};

struct EncoderConfig {
    uint8_t pinA;
    uint8_t pinB;
};

struct RotaryEncoderEventCollection
{
    int id;
    uint64_t events;
    size_t numEvents;
    bool dirty;
    bool overflow;
};

class RotaryEncoderHandler
{
public:
    RotaryEncoderHandler();
    ~RotaryEncoderHandler() = default;
    void init(Adafruit_MCP23X17 &mcpRef, uint8_t intPin, KeypadHandler &keypadHandler, const std::vector<EncoderConfig>& configs);
    void recordEvent(bool clockwise, int id);
    std::vector<RotaryEvent> getEvents(int id, bool &overflow);
    std::vector<RotaryEvent> getEvents(int id);
    std::vector<std::tuple<int, std::vector<RotaryEvent>>> getAllEvents();

private:
    void registerEncoder(int encoderId);
    void registerEncoders(RotaryEncOverMCP *encoders, int numEncoders);
    static void IRAM_ATTR intCallBack();
    static void rotaryReaderTask(void *pArgs);
    static void RotaryEncoderChanged(bool clockwise, int id);
    void handleInterrupt();
    void lock();
    void unlock();
    static RotaryEncoderHandler *instance;
    uint8_t mcpIntPin;
    Adafruit_MCP23X17 *mcp;
    KeypadHandler *keypadHandler;
    std::vector<RotaryEncOverMCP> rotaryEncoders;
    std::map<int, RotaryEncoderEventCollection> _rotaryIDToEvents;
    SemaphoreHandle_t rotaryISRSemaphore = nullptr;
    SemaphoreHandle_t _dataLock;
};
