// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#pragma once

#include <Arduino.h>
#include <SD.h>
#include <CRC32.h>
#include <vector>
#include <optional>
#include "usb/OpCodes.hpp"
#include "usb/Errors.hpp"

namespace hardware
{
    class Storage
    {
    public:
        static constexpr const char *IMAGE_DIR = "/icons/";
        static constexpr const char *PROFILES_DIR = "/profiles/";

        Storage();
        ~Storage();
        void begin();
        void loop();
        static bool validateName(const String &name);
        static std::optional<uint8_t> parsePageId(const String &filename);
        bool fileExists(const String &path);
        std::vector<String> listDir(const String &path, bool onlyDirs = false);
        uint32_t calculateFileCRC(File &file);
        uint32_t freeSpaceKb();
        File openFile(const String &path, const char* mode = FILE_READ);
        usb::ErrorCode createDir(const String &path);
        usb::ErrorCode renameDir(const String &oldPath, const String &newPath);
        usb::ErrorCode deleteDirRecursive(const String &path);
        usb::ErrorCode writeFile(const String &data);
        usb::ErrorCode handleFileChunk(const uint8_t *data, size_t len);
        usb::ErrorCode finishFile(const uint32_t expectedCRC);
        usb::ErrorCode cancelFileTransfer();
        const String &getTransferProfile() const { return transferProfile; }

    private:
        CRC32 hashCrc;
        CRC32 transferCrc;
        File file;
        String currentFilePath = "";
        String currentTempPath = "";
        String transferProfile = "";
        bool receivingFile = false;
        uint32_t lastFileActivityTime = 0;
        uint32_t transferBytes = 0;
        static constexpr uint8_t FILE_CHUNK_MAX_SIZE = 64;
        static constexpr uint32_t FILE_TRANSFER_TIMEOUT = 1000;
        static constexpr uint32_t FILE_TRANSFER_MAX_BYTES = 4 * 1024 * 1024;

        void closeFile();
        void removeFile(const String &path);
    };
}
