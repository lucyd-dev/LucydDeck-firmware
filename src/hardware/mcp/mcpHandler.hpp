#pragma once

#include <Adafruit_MCP23X17.h>

class McpHandler
{
public:
  McpHandler();
  ~McpHandler() = default;

  void initMcp();
  Adafruit_MCP23X17 &getMcp();

private:
  Adafruit_MCP23X17 *mcp;
};
