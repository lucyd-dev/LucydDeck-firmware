#include "keypadHandler.h"

KeypadHandler::KeypadHandler()
{

}

void KeypadHandler::init(Adafruit_MCP23X17 &mcpRef, std::vector<uint8_t> buttonPins)
{
    mcp = &mcpRef;

    for (size_t i = 0; i < buttonPins.size(); i++)
    {
        buttons.resize(buttonPins.size(), Bounce2Mcp());
        rButtonRotation.resize(buttonPins.size(), false);
        buttons[i].attach(mcpRef, buttonPins[i], INPUT_PULLUP);
        buttons[i].interval(25);
    }
}
void KeypadHandler::loop()
{
    int8_t btnPressed = -1;
    for (size_t i = 0; i < buttons.size(); i++)
    {
        buttons[i].update();

        buttons[i].update();
        rButtonRotation[i] = true;

        if (buttons[i].fell())
        {
            rButtonRotation[i] = false;
            continue;
        }
        if (buttons[i].rose())
        {
            if (rButtonRotation[i])
            {
                continue;
            }
            // PacketProcessor::sendCommandPacket(String(i));
        }
    }
}

void KeypadHandler::setRotationFlag(uint8_t btnIndex)
{
    buttons[btnIndex].update();
    rButtonRotation[btnIndex] = true;
}
