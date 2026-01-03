#pragma once

#include <Arduino.h>

enum Command : uint8_t
{
    CMD_VERSION = 0x01,
    CMD_TRIGGER_ACTION = 0x02,
    CMD_CONFIG_UPLOAD = 0x03,
    CMD_IMAGE_UPLOAD = 0x04,
    CMD_FILE_CHUNK = 0x05,
    CMD_FILE_END = 0x06,
    CMD_PAGE = 0x10,
    CMD_PAGE_NEXT = 0x11,
    CMD_PAGE_PREV = 0x12,
    CMD_ACK = 0x7F,
    CMD_ERROR = 0x80
};
