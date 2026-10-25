// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include <Arduino.h>
#include <driver/gpio.h>
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

void render()
{
    displayManager.renderButtonsFromConfig(configManager.getCurrentConfig());
}

void setup()
{
    Serial0.begin(115200);
    Serial0.setDebugOutput(true);
    Serial0.println("--- Setup Start ---");

    // The touch driver installs the GPIO ISR service from inside board.begin(). On this core that
    // install runs esp_intr_alloc() on the 1 KB IPC task, where an interrupt nesting can trip the
    // stack canary and leave the deck booting to a blank screen with no USB. Installing the service
    // here, on the main task and before the panel starts, makes the driver's own call return
    // "already installed", so the allocation never runs on that small stack. The touch still adds
    // its handler to this service.
    // Upstream: espressif/arduino-esp32#12650, fixed in the precompiled libs by
    // esp32-arduino-lib-builder#386. This project pins arduino-esp32 3.1.1, which predates it.
    gpio_install_isr_service(0);

    board.init();
    displayManager.ifAvoidTearing(board.getBoard());
    board.begin();

    // Mount the card and settle any interrupted transfer before USB brings the command surface up:
    // a host that connects during boot must not be able to open a staging file the sweep is about
    // to treat as stale.
    storage.begin();
    configManager.begin();

    usbManager.setPacketCallback([](usb::OpCode opCode, const uint16_t sequence, const uint8_t *data, size_t len)
    {
        dispatcher.onPacket(opCode, sequence, data, len);
    });
    usbManager.begin();

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
            render();
    }
    wasConnected = connected;

    if (displayManager.isRenderPending() && !configManager.getCurrentConfig().empty())
        render();
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
    default:
        return;
    }

    if (seq.empty())
        return;

    usbManager.postSequence(seq);
}
