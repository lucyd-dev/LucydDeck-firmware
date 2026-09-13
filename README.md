# LucydDeck Firmware

The dedicated firmware for the [LucydDeck project](https://github.com/lucyd-dev/LucydDeck). This project is built specifically for the [Waveshare ESP32-S3-Touch-LCD-4.3](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3) development board.

> [!IMPORTANT]
> Work in Progress: This project is currently in early development and is intended as a hobby project. Features and APIs may change.

![Image of the LucydDeck Device](image.jpg)

## 🛠️ Getting Started

This firmware is built using PlatformIO. Ensure you have the PlatformIO CLI installed or use the VS Code Extension.

### 1. Installation

Clone the repository to your local machine:

```bash
git clone https://github.com/lucyd-dev/LucydDeck-firmware.git
cd LucydDeck-firmware
```

### 2. Connect Your Device

The Waveshare board features two separate USB-C ports with distinct roles:

* **USB-Enhanced-SERIAL (CH343):** Required for the initial flash to flash the bootloader and firmware onto a clean board (or for unbricking).

* **Native USB (USB-OTG):** Used for runtime operation, host HID communication, and subsequent firmware updates / serial monitoring once the firmware's native USB CDC stack is active.

Connect the board via the **CH343 port** for your very first flash. To identify the assigned port, run:

```bash
pio device list
```

> [!NOTE]
> PlatformIO usually auto-detects the port, so specifying it manually with `--upload-port` is optional. After the initial flash, you can switch to the Native USB port for normal development and host-app testing.

### 3. Build & Upload

You can build for either the debug or release environment.

Build only:

```bash
pio run -e <debug or release>
```

Build and Upload:

```bash
# Auto-detect port
pio run -e <debug or release> -t upload

# Specify port manually (replace COMX with your port, e.g., COM3 or /dev/ttyUSB0)
pio run -e <debug or release> -t upload --upload-port <COMX>
```

### 4. Monitoring

To view serial output for debugging:

```bash
# Build, Upload, and Monitor
pio run -e <debug or release> -t monitor

# Monitor only
pio device monitor --port <COMX>
```

## 🚀 Host Application Quick Start

LucydDeck acts as a passive device driven by a companion desktop client. Host integrations interact exclusively with the custom **Vendor HID pipe**; do not capture or intercept the OS-level standard HID interfaces (Keyboard, Mouse, Consumer Control).

### Connection Parameters

| Parameter | Value |
|---|---|
| **VID / PID** | `0x303A` : `0x1001` (Espressif Native USB) |
| **Usage Page / Usage** | `0xFF00` / `0x01` |
| **Report ID / Frame Size** | `0x06` / 64 bytes (1-byte ID + 63-byte raw frame) |

---

### Implementation Lifecycle

**[ Enumerate & Open ] ──► [ Handshake ] ──► [ Sync Cache ] ──► [ Provision / Push ] ──► [ Event Loop ]**

1. **Enumerate & Open**
   Scan for `VID 0x303A` / `PID 0x822E` on the front native USB port and open the vendor interface (`Usage Page 0xFF00`, Report ID `0x06`).
2. **Protocol Handshake**
   Send `CMD_PING` (sequence `0`); verify the device answers `RESP_ACK`. Then send `CMD_GET_DEVICE_INFO` and parse the `RESP_DEVICE_INFO` JSON payload for the firmware version (`fw_version`), protocol version, board, and free space.
3. **Cache Reconciliation**
   Issue `CMD_GET_PROFILES_LIST` and `CMD_GET_IMAGES_LIST` to retrieve on-device profile trees and file checksums before performing transfers.
4. **Asset Synchronization**
   Create required profiles using `CMD_PROFILE_CREATE`, then stage missing icons (`.png`) and layout definitions (`<id>.json`) via the [File Transfer Protocol](wiki/File-Transfer-Protocol.md).
5. **Runtime Event Loop**
   * **Active Control:** Dispatch `CMD_SET_ACTIVE_PROFILE` (`<name>`) or `CMD_SET_ACTIVE_PAGE` (`<id>`) to control active UI state from the PC.
   * **Plugin Handling:** Continuously listen for incoming `EVT_ACTION_TRIGGERED` reports to execute custom desktop actions (e.g., OBS scene switches, mute toggles, Discord events).

## 📜 Developer & Integration Documentation

Comprehensive architecture guides, protocol specifications, and schema definitions are maintained in the [LucydDeck Wiki](wiki/Home.md).

* **Host Integration:** Refer to the [USB Protocol](wiki/USB-Protocol.md) for packet structure and the `EVT_ACTION_TRIGGERED` event pipeline.
* **Configuration Management:** Refer to the [Configuration Schema](wiki/Configuration-Schema.md) and [File Transfer Protocol](wiki/File-Transfer-Protocol.md) for managing profiles, grid layouts, and button assets.

## 📦 Dependencies

This project relies on the following key libraries to interface with the Waveshare hardware:

* [Arduino Core for ESP32](https://github.com/espressif/arduino-esp32) ([LGPL-2.1](https://github.com/espressif/arduino-esp32/blob/master/LICENSE.md))
* [ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel) ([Apache-2.0](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/license.txt))
* [ESP32_IO_Expander](https://github.com/esp-arduino-libs/ESP32_IO_Expander) ([Apache-2.0](https://github.com/esp-arduino-libs/ESP32_IO_Expander/blob/master/license.txt))
* [esp-lib-utils](https://github.com/esp-arduino-libs/esp-lib-utils) ([Apache-2.0](https://github.com/esp-arduino-libs/esp-lib-utils/blob/master/license.txt))
* [LVGL](https://github.com/lvgl/lvgl) ([MIT](https://github.com/lvgl/lvgl/blob/master/LICENCE.txt))
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson) ([MIT](https://github.com/bblanchon/ArduinoJson/blob/7.x/LICENSE.txt))
* [CRC32](https://github.com/bakercp/CRC32) ([MIT](https://github.com/bakercp/CRC32/blob/master/LICENSE.md))

## 🤝 Contributing

If you find this project interesting or useful, contributions are highly welcome! Please feel free to leave a star ⭐ on the repo or submit a Pull Request.

## 📄 License

This project is licensed under the GPL-3.0 License - see the [LICENSE.md](LICENSE.md) file for details.

`Made with 💜 by Lucyd since 2024`
