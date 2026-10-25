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

        // Identity grammars shared by CMD_FILE_START and the delete commands. The host names an
        // entity; the firmware owns the storage path and appends the extension.
        struct PageIdentity
        {
            String profile;
            uint8_t id = 0;
        };
        static usb::ErrorCode parsePageIdentity(const String &identity, PageIdentity &outPage);
        static bool isIconIdentity(const String &identity);
        static String pagePath(const PageIdentity &page);
        static String iconPath(const String &iconName);
        static String profilePath(const String &profile);

        // What CMD_FILE_START resolved to. The transfer handler repaints the entity a committed
        // file belongs to, without parsing the identity a second time.
        struct TransferTarget
        {
            usb::TargetType type = usb::TargetType::TARGET_PAGE;
            String profile;      // TARGET_PAGE: the profile holding the page
            uint8_t pageId = 0;  // TARGET_PAGE
            String iconName;     // TARGET_ICON
        };

        bool fileExists(const String &path);
        bool dirExists(const String &path);
        std::vector<String> listDir(const String &path, bool onlyDirs = false);
        uint32_t calculateFileCRC(File &file);
        uint32_t freeSpaceKb();
        File openFile(const String &path, const char* mode = FILE_READ);
        usb::ErrorCode createDir(const String &path);
        usb::ErrorCode renameDir(const String &oldPath, const String &newPath);
        usb::ErrorCode deleteDirRecursive(const String &path);
        usb::ErrorCode deleteFile(const String &path);
        usb::ErrorCode writeFile(const String &data);
        usb::ErrorCode handleFileChunk(const uint8_t *data, size_t len);
        usb::ErrorCode finishFile(const uint32_t expectedCRC);
        usb::ErrorCode cancelFileTransfer();
        const TransferTarget &getTransferTarget() const { return transferTarget; }

    private:
        CRC32 hashCrc;
        CRC32 transferCrc;
        File file;
        String currentFilePath = "";
        String currentTempPath = "";
        TransferTarget transferTarget;
        bool receivingFile = false;
        uint32_t lastFileActivityTime = 0;
        uint32_t transferBytes = 0;
        static constexpr uint8_t FILE_CHUNK_MAX_SIZE = 64;
        static constexpr uint32_t FILE_TRANSFER_TIMEOUT = 1000;
        static constexpr uint32_t FILE_TRANSFER_MAX_BYTES = 4 * 1024 * 1024;

        void closeFile();
        void removeFile(const String &path);
        static std::optional<uint8_t> parsePageIdValue(const String &id);
        static bool hasTransferExtension(const String &name);
    };
}
