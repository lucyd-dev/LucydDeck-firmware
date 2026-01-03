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

Connect your ESP32-S3 via the USB-Enhanced-SERIAL (CH343) port.

To identify which COM port your device is connected to, run:

```bash
pio device list
```

> [!NOTE]
> PlatformIO usually auto-detects the port, so specifying it is optional to upload or monitor.

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

## 📦 Dependencies

This project relies on the following key libraries to interface with the Waveshare hardware:

- [ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel) – Display driver implementation
- [ESP32_IO_Expander](https://github.com/esp-arduino-libs/ESP32_IO_Expander) – IO expansion handling
- [LVGL](https://github.com/lvgl/lvgl) – Light and Versatile Graphics Library
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson) – JSON serialization/deserialization
- [CRC32](https://github.com/bakercp/CRC32) – Cyclic Redundancy Check

## 🤝 Contributing

If you find this project interesting or useful, contributions are highly welcome! Please feel free to leave a star ⭐ on the repo or submit a Pull Request.

## 📄 License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.

`Made with 💜 by Lucyd since 2026`
