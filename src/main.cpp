#include <Arduino.h>
#include <Wire.h>
#include "display/Display.hpp"
#include "LucydDeckController.hpp"

LucydDeckController lucydDeck("configs/", "icons/");

void setup()
{
    Wire.begin(I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    Serial0.begin(115200);
    Serial0.setDebugOutput(true);
    Serial0.println();
    Serial0.println("|||||||||||||||||||||||||");
    Serial0.println("|||     LucydDeck     |||");
    Serial0.println("|||||||||||||||||||||||||");
    Serial0.println();

    lucydDeck.begin();
}

void loop()
{
    lucydDeck.loop();
}
