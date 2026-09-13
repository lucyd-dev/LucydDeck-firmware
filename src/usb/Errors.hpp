// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>

namespace usb
{
    enum class ErrorCode : uint8_t
    {
        OK                     = 0x00,
        ERR_SEQUENCE           = 0x01, // host frame not exactly prev+1, or M-bit set
        ERR_UNKNOWN_COMMAND    = 0x02,
        ERR_INVALID_DIRECTION  = 0x03, // host used a device-direction opcode (bit 7)
        ERR_INVALID_PATH       = 0x10, // validateName() failed
        ERR_INVALID_PAGE_NAME  = 0x11, // page target not <id>.json
        ERR_PROFILE_NOT_FOUND  = 0x12, // dir missing (upload, rename src, delete)
        ERR_PROFILE_EXISTS     = 0x13, // create or rename target conflicts
        ERR_CREATE             = 0x14, // SD.mkdir failed
        ERR_RENAME             = 0x15, // SD.rename failed
        ERR_DELETE             = 0x16, // recursive delete failed
        ERR_TRANSFER_BUSY      = 0x20, // FILE_START while transfer active
        ERR_INVALID_PATH_TYPE  = 0x21,
        ERR_FILE_OPEN          = 0x22, // .tmp open failed
        ERR_CHUNK_TOO_LARGE    = 0x23,
        ERR_FILE_NOT_OPEN      = 0x24, // CHUNK with no active transfer
        ERR_WRITE              = 0x25,
        ERR_NO_TRANSFER        = 0x26, // FILE_END/CANCEL with none active
        ERR_CRC_MISSING        = 0x27,
        ERR_CRC                = 0x28,
        ERR_FINALIZE           = 0x29, // .tmp → target rename failed
        ERR_UNKNOWN_ACTION     = 0x30, // SET_ACTIVE_PAGE payload not a valid page id
        ERR_PAGE_LOAD          = 0x31,
        ERR_PROFILE_LOAD       = 0x32,
    };
}
