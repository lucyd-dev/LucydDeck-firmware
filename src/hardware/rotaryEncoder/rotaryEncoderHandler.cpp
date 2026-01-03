#include "rotaryEncoderHandler.hpp"

RotaryEncoderHandler *RotaryEncoderHandler::instance = nullptr;

RotaryEncoderHandler::RotaryEncoderHandler()
{
    instance = this;
    mcp = nullptr;
    mcpIntPin = 0;
    keypadHandler = nullptr;
}

void RotaryEncoderHandler::init(Adafruit_MCP23X17 &mcpRef, uint8_t intPin, KeypadHandler &kypHandler, const std::vector<EncoderConfig>& configs)
{
    mcp = &mcpRef;
    keypadHandler = &kypHandler;
    mcpIntPin = intPin;
    rotaryISRSemaphore = xSemaphoreCreateBinary();
    pinMode(mcpIntPin, INPUT);
    mcp->setupInterrupts(true, false, LOW);

    rotaryEncoders.reserve(configs.size());
    for (size_t i = 0; i < configs.size(); i++) {
        const auto& config = configs[i];
        rotaryEncoders.emplace_back(mcp, config.pinA, config.pinB, RotaryEncoderChanged, i);
        rotaryEncoders.back().init();
    }

    _dataLock = xSemaphoreCreateRecursiveMutex();
    registerEncoders(rotaryEncoders.data(), rotaryEncoders.size());
    xTaskCreatePinnedToCore(rotaryReaderTask, "rotary reader", 2048, NULL, 20, NULL, 1);
    attachInterrupt(mcpIntPin, intCallBack, FALLING);
}

void RotaryEncoderHandler::registerEncoder(int encoderId)
{
    _rotaryIDToEvents[encoderId] = RotaryEncoderEventCollection{encoderId};
}

void RotaryEncoderHandler::registerEncoders(RotaryEncOverMCP *encoders, int numEncoders)
{
    for (int i = 0; i < numEncoders; i++)
    {
        registerEncoder(encoders[i].getID());
    }
}

void RotaryEncoderHandler::lock()
{
    xSemaphoreTakeRecursive(_dataLock, portMAX_DELAY);
}

void RotaryEncoderHandler::unlock()
{
    xSemaphoreGiveRecursive(_dataLock);
}

void RotaryEncoderHandler::recordEvent(bool clockwise, int id)
{
    lock();
    if (!_rotaryIDToEvents.count(id))
    {
        unlock();
        return;
    }
    auto &record = _rotaryIDToEvents[id];
    if (record.numEvents == 64)
    {
        record.overflow = true;
    }
    else
    {
        if (!clockwise)
            record.events |= (uint64_t)(1ull << record.numEvents);
        record.numEvents++;
        record.dirty = true;
    }
    unlock();
}

std::vector<RotaryEvent> RotaryEncoderHandler::getEvents(int id, bool &overflow)
{
    lock();
    std::vector<RotaryEvent> events{};
    if (!_rotaryIDToEvents.count(id))
    {
        unlock();
        return events;
    }
    auto &record = _rotaryIDToEvents[id];
    if (record.dirty)
    {
        for (size_t i = 0; i < record.numEvents; i++)
        {
            RotaryEvent evt = ((record.events & (1ull << i)) == 0) ? RotaryEvent::RotationClockwise : RotaryEvent::RotationCounterClockwise;
            events.push_back(evt);
        }
        record.dirty = false;
        overflow = record.overflow;
        record.overflow = false;
        record.numEvents = 0;
        record.events = 0ul;
    }
    unlock();
    return events;
}

std::vector<RotaryEvent> RotaryEncoderHandler::getEvents(int id)
{
    bool dummy = false;
    return getEvents(id, dummy);
}

std::vector<std::tuple<int, std::vector<RotaryEvent>>> RotaryEncoderHandler::getAllEvents()
{
    std::vector<std::tuple<int, std::vector<RotaryEvent>>> events{};
    lock();
    for (auto &p : _rotaryIDToEvents)
    {
        auto new_events = getEvents(p.first);
        if (!new_events.empty())
            events.push_back(std::make_tuple(p.first, new_events));
    }
    unlock();
    return events;
}

void IRAM_ATTR RotaryEncoderHandler::intCallBack()
{
    if(instance == nullptr) return;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(instance->rotaryISRSemaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

void RotaryEncoderHandler::rotaryReaderTask(void *pArgs)
{
    if(instance == nullptr) return;

    (void)pArgs;
    Serial0.println("Started rotary reader task.");
    while (true)
    {
        if (xSemaphoreTake(instance->rotaryISRSemaphore, portMAX_DELAY) == pdPASS)
        {
            auto gpioAB = instance->mcp->getCapturedInterrupt();
            volatile uint16_t dummy = instance->mcp->readGPIOAB();
            for (auto &rEncoder : instance->rotaryEncoders)
            {
                rEncoder.feedInput(gpioAB);
            }

        }
    }
}

void RotaryEncoderHandler::handleInterrupt()
{
    auto gpioAB = mcp->getCapturedInterrupt();
    volatile uint16_t dummy = mcp->readGPIOAB();
    for (auto &rEncoder : rotaryEncoders)
    {
        if (rEncoder.getMCP() == mcp)
            rEncoder.feedInput(gpioAB);
    }
}

void RotaryEncoderHandler::RotaryEncoderChanged(bool clockwise, int id)
{
    if(instance == nullptr) return;

    instance->keypadHandler->setRotationFlag(id);
    instance->recordEvent(clockwise, id);
    // PacketProcessor::sendCommandPacket("Encoder changed " + String(clockwise) + " " + String(id));
}
