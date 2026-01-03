#pragma once

#include <Adafruit_MCP23X17.h>
#include <Bounce2.h>

class Bounce2Mcp : public Bounce {
public:
  Bounce2Mcp();
  ~Bounce2Mcp() = default;
  void attach(Adafruit_MCP23X17 mcp, int pin, int mode);
  void attach(Adafruit_MCP23X17 mcp, int pin);

protected:
  Adafruit_MCP23X17 _mcpX;
  virtual bool readCurrentState() override;
  virtual void setPinMode(int pin, int mode) override;
};
