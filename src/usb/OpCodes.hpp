// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>

namespace usb
{
    // Wire Protocol opcode table (authoritative — see wiki/ USB-Protocol.md).
    // Host→Device opcodes have bit 7 = 0; Device→Host opcodes have bit 7 = 1.

    enum OpCode : uint8_t
    {
        // 0x0X device info (host→device)
        CMD_PING = 0x00,            // heartbeat, no payload → response: RESP_ACK
        CMD_GET_DEVICE_INFO = 0x01, // response: RESP_DEVICE_INFO
        CMD_GET_IMAGES_LIST = 0x02,      // response: RESP_IMAGES_LIST
        CMD_GET_PROFILES_LIST = 0x03,   // response: RESP_PROFILES_LIST

        // 0x2X profile operations (host→device)
        CMD_PROFILE_CREATE = 0x20, // data: profile name (string)
        CMD_PROFILE_RENAME = 0x21, // data: old:new (string)
        CMD_PROFILE_DELETE = 0x22, // data: profile name (string)

        // 0x3X file transfer (host→device)
        CMD_FILE_START = 0x30,     // data: pathType(1 byte) + path (string)
        CMD_FILE_CHUNK = 0x31,     // data: binary chunk (≤ 60 bytes)
        CMD_FILE_END = 0x32,       // data: CRC32 of received bytes, big-endian (4 bytes)
        CMD_FILE_CANCEL = 0x33,    // data: none

        // 0x4X navigation (host→device)
        CMD_SET_ACTIVE_PROFILE = 0x40, // data: profile name (string)
        CMD_SET_ACTIVE_PAGE = 0x41,    // data: page id (string)

        // 0x8X device info responses (device→host)
        RESP_DEVICE_INFO = 0x80,   // data: JSON object (fw_version, protocol_version, board, free_space_kb)
        RESP_IMAGES_LIST = 0x81,    // data: JSON array (may be M-bit stream)
        RESP_PROFILES_LIST = 0x82, // data: JSON array (may be M-bit stream)

        // 0xAX events (device→host, unsolicited)
        EVT_ACTION_TRIGGERED = 0xA0, // data: action string verbatim

        // 0xFX general responses (device→host)
        RESP_ACK = 0xFF,           // header-only ACK (zero-length payload)
        RESP_ERROR = 0xFE          // data: 1-byte ErrorCode (see Errors.hpp)
    };

    // FILE_START path grammars
    enum PathType : uint8_t
    {
        PATH_PAGE = 0x01,          // "<profile>/<page.json>" — split on first '/', both halves pass
                                   // validateName, page half matches ^[0-9]+\.json$, profile dir must exist.
        PATH_ICON = 0x02           // "<name>" — flat validateName, target /icons/<name>.
    };
}
