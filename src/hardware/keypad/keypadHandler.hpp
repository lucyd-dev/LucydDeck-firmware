#pragma once

#include <Bounce2Mcp.h>
#include <vector>
#include "hardware/bounce2Mcp/bounce2Mcp.hpp"
#include "network/packetProcessor.hpp"

class KeypadHandler {
public:
    KeypadHandler();
    ~KeypadHandler() = default;
    void init(Adafruit_MCP23X17 &mcpRef, std::vector<uint8_t> buttonPins);
    void loop();
    void setRotationFlag(uint8_t btnIndex);

private:
    Adafruit_MCP23X17 *mcp;
    std::vector<Bounce2Mcp> buttons;
    std::vector<bool> rButtonRotation;
};
