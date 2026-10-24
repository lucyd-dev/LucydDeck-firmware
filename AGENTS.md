# LucydDeck-firmware

The ESP32-S3 firmware for the LucydDeck, a 15-key stream deck built on the Waveshare ESP32-S3-Touch-LCD-4.3. PlatformIO with the Arduino framework. The host app is in `lucyd-dev/LucydDeck`.

## Agent skills

### Issue tracker

GitHub Issues in this repository, and in `lucyd-dev/LucydDeck` for the host side. See `docs/agents/issue-tracker.md`.

### Domain docs

The `wiki/` directory is the source of truth for the wire, the SD card layout and the config schema. Read the pages that touch the area before changing it.

## Build and flash

PlatformIO is installed in WSL at `~/.local/bin/pio`. The deck is attached to WSL with usbipd, so flashing also happens here.

```bash
pio run -e debug              # build
pio run -e debug -t upload    # flash
pio device monitor -e debug   # serial log
pio run -e release            # release build
```

The Windows clone at `/mnt/c/Users/steve/Documents/Code/LucydDeckProject/LucydDeck-firmware` is a fallback only. Do not develop there.

## Layout

- `src/usb/`: framing, sequence, dispatcher, error codes, opcodes, and the handlers for profiles, file transfer, queries and device info.
- `src/hardware/`: storage and SD card access, USB, board support.
- `src/config/`: `ConfigManager`, actions, page config.
- `src/core/`: `DeckController`.
- `src/display/`: LVGL display layer.
- `wiki/`: wire protocol, file transfer, configuration schema, storage structure.

## Rules

1. Conventional Commits.
2. No push, pull request or GitHub mutation without explicit user confirmation.
3. A wire change updates the wiki in the same change. Never guess an opcode or a field.
4. The project stays on `protocol_version` 1 until the 1.0.0 release. New commands join protocol 1; there are no per-change bumps.
5. The host adapts to the firmware and the wiki, not the other way around.
6. Address entities by identity on the wire: profiles by name, pages by `<profile>/<id>`, icons by `<name>`. The host never names a file or an extension; the firmware builds the storage path.
