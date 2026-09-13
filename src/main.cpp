// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include <Arduino.h>
#include "usb/UsbManager.hpp"
#include "hardware/Board.hpp"
#include "hardware/Storage.hpp"
#include "config/ConfigManager.hpp"
#include "display/DisplayManager.hpp"
#include "core/DeckController.hpp"
#include "usb/protocol/Dispatcher.hpp"

using namespace config::actions;

hardware::Board board;
hardware::Storage storage;
display::DisplayManager displayManager(storage);
usb::UsbManager usbManager;
config::ConfigManager configManager(storage);
core::DeckController deckController(configManager, displayManager);
usb::protocol::Dispatcher dispatcher(usbManager, storage, configManager, deckController);

void onButtonPress(uint8_t id, lv_event_code_t code);
void render();

void render()
{
    displayManager.renderButtonsFromConfig(configManager.getCurrentConfig());
}

void setup()
{
    Serial0.begin(115200);
    Serial0.setDebugOutput(true);
    Serial0.println("--- Setup Start ---");

    board.init();
    displayManager.ifAvoidTearing(board.getBoard());
    board.begin();

    usbManager.setPacketCallback([](usb::OpCode opCode, const uint16_t sequence, const uint8_t *data, size_t len)
    {
        dispatcher.onPacket(opCode, sequence, data, len);
    });
    usbManager.begin();

    storage.begin();
    configManager.begin();

    displayManager.setButtonCallback(&onButtonPress);
    displayManager.initializeLVGL(board.getBoard());
    displayManager.renderStartupScreen();

    Serial0.println("--- Setup Complete ---");
    Serial0.println("LucydDeck Firmware started");
    Serial0.printf("Firmware Version: %s\n", FW_VERSION);
    Serial0.printf("Protocol Version: %u\n", static_cast<unsigned int>(PROTOCOL_VERSION));
    Serial0.printf("Board: %s\n", BOARD_NAME);

    delay(800);
    render();

}

bool wasConnected = false;

void loop()
{
    usbManager.loop();
    storage.loop();

    ActionData internalAction;
    while (usbManager.takeInternal(internalAction))
        deckController.handleInternal(internalAction);

    const bool connected = usbManager.isConnected();
    if (wasConnected && !connected)
    {
        Serial0.println("USB disconnected, reverting to splash screen");
        displayManager.renderStartupScreen();
    }
    else if (!wasConnected && connected)
    {
        if (!configManager.getCurrentConfig().empty())
            displayManager.renderButtonsFromConfig(configManager.getCurrentConfig());
    }
    wasConnected = connected;

    if (displayManager.isRenderPending() && !configManager.getCurrentConfig().empty())
        displayManager.renderButtonsFromConfig(configManager.getCurrentConfig());
}

void onButtonPress(uint8_t id, lv_event_code_t code)
{
    const PageConfig &config = configManager.getCurrentConfig();
    auto it = config.find(id);
    if (it == config.end())
        return;

    const ButtonConfig &btnCfg = it->second;

    ActionSequence seq;
    switch (code)
    {
    case LV_EVENT_SHORT_CLICKED:
        seq = btnCfg.click;
        break;
    case LV_EVENT_LONG_PRESSED:
        seq = btnCfg.longPress;
        break;
    default:
        return;
    }

    if (seq.empty())
        return;

#if CORE_DEBUG_LEVEL >= 4
    const char *eventName = (code == LV_EVENT_LONG_PRESSED) ? "LONG_PRESSED" : "SHORT_CLICKED";
    Serial0.printf("[BTN] id=%u code=%s seq=%u\n", id, eventName, (unsigned int)seq.size());
#endif

    usbManager.postSequence(seq);
}
