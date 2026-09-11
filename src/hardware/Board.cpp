// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "Board.hpp"
#include <USB.h>
#include <SPI.h>

namespace hardware
{

    esp_expander::Base *Board::expander = nullptr;
    esp_panel::board::Board *Board::board = nullptr;

    Board::Board()
    {
    }
    
    Board::~Board()
    {
        if (board == nullptr)
            return;
        
        Serial0.println("Deinitializing board...");
        board->del();
        delete board;
        board = nullptr;
    }

    void Board::init()
    {
        if (board != nullptr)
        {
            Serial0.println("Board already initialized!");
            return;
        }

        Serial0.println("Initialize Board");
        board = new esp_panel::board::Board();
        board->init();
    }

    void Board::begin()
    {
        if (board == nullptr)
        {
            Serial0.println("[ERR] Board::begin() called before Board::init()");
            return;
        }

        assert(board->begin());
        initExpander();
        Serial0.println("Board initialized successfully");
    }

    void Board::initExpander()
    {
        Serial0.println("Initialize IO Expander");
        expander = board->getIO_Expander()->getBase();
        expander->digitalWrite(USB_SEL, LOW);
        expander->digitalWrite(SD_CS, LOW);
    }

}
