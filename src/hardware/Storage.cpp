// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2024 LucydDev

#include "Storage.hpp"
#include <USB.h>

namespace hardware
{

    Storage::Storage()
    {
    }
    
    Storage::~Storage()
    {
        SPI.end();
    }

    void Storage::begin()
    {
        Serial0.println("Initialize SD Card");
        SPI.setHwCs(false);
        SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_SS);
        SD.begin(SD_CS);

        uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE)
        {
            Serial0.println("No SD card attached");
            return;
        }

        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial0.printf("SD Card Size: %lluMB\n", cardSize);
    }

    bool Storage::validateName(const String &name)
    {
        if (name.isEmpty())
            return false;
        if (name == "." || name == "..")
            return false;
        for (unsigned int i = 0; i < name.length(); ++i)
        {
            char c = name[i];
            if (c == '/' || c == '\\')
                return false;
            if (c < 0x20)
                return false;
        }
        return true;
    }

    std::optional<uint8_t> Storage::parsePageId(const String &filename)
    {
        int dot = filename.indexOf('.');
        if (dot <= 0)
            return std::nullopt;

        String idPart = filename.substring(0, dot);
        String extPart = filename.substring(dot);
        if (extPart != ".json" || idPart.length() > 3)
            return std::nullopt;

        for (unsigned int i = 0; i < idPart.length(); ++i)
        {
            if (idPart[i] < '0' || idPart[i] > '9')
                return std::nullopt;
        }

        long id = idPart.toInt();
        if (id < 0 || id > 255)
            return std::nullopt;
        return static_cast<uint8_t>(id);
    }

    bool Storage::fileExists(const String &path)
    {
        return SD.exists(path.c_str());
    }    
    
    std::vector<String> Storage::listDir(const String &dir, bool onlyDirs)
    {
        std::vector<String> fileList;
        File root = SD.open(dir.c_str());
        if (!root || !root.isDirectory())
            return fileList;

        File file = root.openNextFile();
        while (file)
        {
            if (!onlyDirs || (onlyDirs && file.isDirectory()))
            {
                fileList.push_back(String(file.name()));
            }
            file = root.openNextFile();
        }
        root.close();
        return fileList;
    }
    
    

    uint32_t Storage::calculateFileCRC(File &file)
    {
        hashCrc.reset();
        uint8_t buffer[FILE_CHUNK_MAX_SIZE];
        while (file.available())
        {
            size_t bytesRead = file.read(buffer, sizeof(buffer));
            hashCrc.update(buffer, bytesRead);
        }
        return hashCrc.finalize();
    }

    File Storage::openFile(const String &path, const char* mode)
    {
        return SD.open(path.c_str(), mode);
    }

    usb::ErrorCode Storage::createDir(const String &path)
    {
        if (SD.exists(path.c_str()))
            return usb::ErrorCode::OK;

        if (!SD.mkdir(path.c_str()))
        {
            Serial0.printf("[ERR] Failed to create dir: %s\n", path.c_str());
            return usb::ErrorCode::ERR_CREATE;
        }

        Serial0.printf("Dir created: %s\n", path.c_str());
        return usb::ErrorCode::OK;
    }
    
    usb::ErrorCode Storage::renameDir(const String &oldPath, const String &newPath)
    {
        if (!SD.exists(oldPath.c_str()))
            return usb::ErrorCode::ERR_PROFILE_NOT_FOUND;

        if (SD.exists(newPath.c_str()))
            return usb::ErrorCode::ERR_PROFILE_EXISTS;

        if (!SD.rename(oldPath.c_str(), newPath.c_str()))
        {
            Serial0.printf("[ERR] Failed to rename dir from %s to %s\n", oldPath.c_str(), newPath.c_str());
            return usb::ErrorCode::ERR_RENAME;
        }

        Serial0.printf("Dir renamed from %s to %s\n", oldPath.c_str(), newPath.c_str());
        return usb::ErrorCode::OK;
    }

    usb::ErrorCode Storage::deleteDirRecursive(const String &path)
    {
        if (!SD.exists(path.c_str()))
            return usb::ErrorCode::ERR_PROFILE_NOT_FOUND;

        File dir = SD.open(path.c_str());
        if (!dir)
            return usb::ErrorCode::ERR_DELETE;
        if (!dir.isDirectory())
        {
            dir.close();
            return usb::ErrorCode::ERR_DELETE;
        }

        File entry = dir.openNextFile();
        while (entry)
        {
            String entryPath = path + "/" + String(entry.name());
            if (entry.isDirectory())
            {
                if (deleteDirRecursive(entryPath) != usb::ErrorCode::OK)
                {
                    dir.close();
                    return usb::ErrorCode::ERR_DELETE;
                }
            }
            else
            {
                if (!SD.remove(entryPath.c_str()))
                {
                    dir.close();
                    return usb::ErrorCode::ERR_DELETE;
                }
            }
            entry = dir.openNextFile();
        }
        dir.close();

        if (!SD.rmdir(path.c_str()))
            return usb::ErrorCode::ERR_DELETE;

        Serial0.printf("Dir deleted: %s\n", path.c_str());
        return usb::ErrorCode::OK;
    }
    
    usb::ErrorCode Storage::writeFile(const String &data)
    {
        if (receivingFile)
        {
            Serial0.println("[ERR] Already receiving file");
            return usb::ErrorCode::ERR_TRANSFER_BUSY;
        }

        if (file)
            file.close();

        if (data.isEmpty())
            return usb::ErrorCode::ERR_INVALID_PATH_TYPE;

        auto pathType = static_cast<usb::PathType>(data[0]);
        if (pathType != usb::PathType::PATH_PAGE && pathType != usb::PathType::PATH_ICON)
        {
            Serial0.printf("[ERR] Invalid path type: 0x%02X\n", data[0]);
            return usb::ErrorCode::ERR_INVALID_PATH_TYPE;
        }

        String rest = data.substring(1);
        String targetPath;
        String transferProfile = "";

        if (pathType == usb::PathType::PATH_PAGE)
        {
            int slash = rest.indexOf('/');
            if (slash <= 0 || slash >= rest.length() - 1)
                return usb::ErrorCode::ERR_INVALID_PATH;

            String profile = rest.substring(0, slash);
            String filename = rest.substring(slash + 1);
            if (!validateName(profile) || !validateName(filename))
                return usb::ErrorCode::ERR_INVALID_PATH;

            if (!parsePageId(filename).has_value())
                return usb::ErrorCode::ERR_INVALID_PAGE_NAME;

            String profileDir = PROFILES_DIR + profile;
            if (!SD.exists(profileDir.c_str()))
                return usb::ErrorCode::ERR_PROFILE_NOT_FOUND;

            targetPath = profileDir + "/" + filename;
            transferProfile = profile;
        }
        else
        {
            if (!validateName(rest))
                return usb::ErrorCode::ERR_INVALID_PATH;
            targetPath = IMAGE_DIR + rest;
        }

        String tempPath = targetPath + ".tmp";
        file = SD.open(tempPath.c_str(), FILE_WRITE);
        if (!file)
        {
            Serial0.printf("[ERR] Failed to open file: %s\n", tempPath.c_str());
            return usb::ErrorCode::ERR_FILE_OPEN;
        }

        receivingFile = true;
        lastFileActivityTime = millis();
        transferBytes = 0;
        currentFilePath = targetPath;
        currentTempPath = tempPath;
        this->transferProfile = transferProfile;
        transferCrc.reset();
        Serial0.printf("Creating new file: %s\n", targetPath.c_str());
        return usb::ErrorCode::OK;
    }

    usb::ErrorCode Storage::handleFileChunk(const uint8_t *data, size_t len)
    {
        if (len > FILE_CHUNK_MAX_SIZE)
        {
            Serial0.println("[ERR] Chunk too large");
            return usb::ErrorCode::ERR_CHUNK_TOO_LARGE;
        }

        if (!receivingFile || !file)
        {
            Serial0.println("[ERR] No file open for writing");
            return usb::ErrorCode::ERR_FILE_NOT_OPEN;
        }

        if (len > FILE_TRANSFER_MAX_BYTES - transferBytes)
        {
            Serial0.println("[ERR] Transfer exceeded size budget");
            String tempPath = currentTempPath;
            closeFile();
            removeFile(tempPath);
            receivingFile = false;

            return usb::ErrorCode::ERR_CHUNK_TOO_LARGE;
        }

        lastFileActivityTime = millis();
        size_t written = file.write(data, len);
        if (written != len)
        {
            Serial0.println("[ERR] Failed to write to file");
            String tempPath = currentTempPath;
            closeFile();
            removeFile(tempPath);

            return usb::ErrorCode::ERR_WRITE;
        }

        transferBytes += static_cast<uint32_t>(len);
        transferCrc.update(data, len);

        return usb::ErrorCode::OK;
    }

    void Storage::loop()
    {
        if (receivingFile && (millis() - lastFileActivityTime > FILE_TRANSFER_TIMEOUT))
        {
            Serial0.println("[ERR] File transfer timeout, aborting receive...");
            String tempPath = currentTempPath;
            closeFile();
            removeFile(tempPath);
        }
    }


    usb::ErrorCode Storage::finishFile(const uint32_t expectedCRC)
    {
        if (!receivingFile)
        {
            Serial0.println("[ERR] No file transfer in progress");
            return usb::ErrorCode::ERR_NO_TRANSFER;
        }

        String targetPath = currentFilePath;
        String tempPath = currentTempPath;
        closeFile();

        uint32_t calculatedCRC = transferCrc.finalize();
        if (calculatedCRC != expectedCRC)
        {
            Serial0.printf("[ERR] CRC mismatch: expected 0x%08X, calculated 0x%08X\n", expectedCRC, calculatedCRC);

            removeFile(tempPath);
            return usb::ErrorCode::ERR_CRC;
        }

        if (!SD.rename(tempPath.c_str(), targetPath.c_str()))
        {
            Serial0.printf("[ERR] Failed to finalize file: %s\n", targetPath.c_str());
            removeFile(tempPath);
            return usb::ErrorCode::ERR_FINALIZE;
        }

        Serial0.println("File saved successfully");
        return usb::ErrorCode::OK;
    }

    usb::ErrorCode Storage::cancelFileTransfer()
    {
        if (!receivingFile)
        {
            Serial0.println("[ERR] No file transfer in progress");
            return usb::ErrorCode::ERR_NO_TRANSFER;
        }

        Serial0.println("[WARN] File transfer cancelled");
        String tempPath = currentTempPath;
        closeFile();
        removeFile(tempPath);
        return usb::ErrorCode::OK;
    }

    void Storage::closeFile()
    {
        receivingFile = false;
        currentFilePath = "";
        currentTempPath = "";
        if (file)
        {
            file.close();
            file = File();
        }
    }

    void Storage::removeFile(const String &path)
    {
        if (SD.exists(path.c_str()))
        {
            SD.remove(path.c_str());
            Serial0.printf("Removed file: %s\n", path.c_str());
        }
    }

}
