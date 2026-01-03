#include "bounce2Mcp.h"

Bounce2Mcp::Bounce2Mcp() {}

void Bounce2Mcp::attach(Adafruit_MCP23X17 mcp, int pin, int mode) {
  _mcpX = mcp;
  setPinMode(pin, mode);
  Bounce::attach(pin);
}

void Bounce2Mcp::attach(Adafruit_MCP23X17 mcp, int pin) {
  _mcpX = mcp;
  Bounce::attach(pin);
}

bool Bounce2Mcp::readCurrentState() {
  return _mcpX.digitalRead(pin);
}

void Bounce2Mcp::setPinMode(int pin, int mode) {
  _mcpX.pinMode(pin, mode);
}
