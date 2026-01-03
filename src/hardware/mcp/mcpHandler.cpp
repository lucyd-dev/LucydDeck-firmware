#include "mcpHandler.hpp"

McpHandler::McpHandler()
{
}

void McpHandler::initMcp()
{
    Serial0.println("Initializing MCP23017 expander");
    if (mcp != nullptr) {
        delete mcp;
    }
    mcp = new Adafruit_MCP23X17();

    if (!mcp->begin_I2C(0x24)) {
        Serial0.println("Failed to initialize MCP23017");
        return;
    }

    Serial0.println("MCP23017 initialized successfully at address 0x24");
}

Adafruit_MCP23X17 &McpHandler::getMcp()
{
    return *mcp;
}
