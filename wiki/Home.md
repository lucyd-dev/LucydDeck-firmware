# Home

## What is LucydDeck?

**LucydDeck** is a compact 15-key streamer / creator deck. It is a standalone USB device that renders a grid of icon buttons on a touchscreen. When a button is pressed it performs a sequence of "actions": it can type keys, emit consumer-control (multimedia) codes, move/click the mouse, switch pages/profiles, or — most importantly for a host app — **relay plugin commands to the PC over a vendor HID pipe** so a host-side plugin layer (Discord, OBS, etc.) can act on them.

The on-device state is driven entirely by files on a microSD card:

- **Page configs** – JSON files describing the button grid for a given page.
- **Icon images** – PNG files rendered on buttons.
- **Profiles** – directories that group pages.

This repository is the **firmware only**. There is no bundled PC host; the host app is expected to:
1. Enumerate and open the device's **vendor HID interface** (report ID `0x06`).
2. Speak the packet protocol described in [USB-Protocol](USB-Protocol).
3. Upload page configs and icons via [File-Transfer-Protocol](File-Transfer-Protocol).
4. Receive `EVT_ACTION_TRIGGERED` events and execute host/plugin actions.

## Hardware Connection Specifications

| Property | Value | Source |
|---|---|---|
| Board | Waveshare ESP32-S3-Touch-LCD-4.3 | board variant `waveshare_esp32_s3_touch_lcd_43` |
| Top USB | **USB-Enhanced-Serial (CH343)** – programming / serial monitor only | board variant |
| Bottom USB | **Native USB (USB-OTG)** – carries the HID interfaces used by the protocol | board variant |
| USB Vendor ID (`USB_VID`) | `0x303A` (Espressif Systems) | board variant `pins_arduino.h` |
| USB Product ID (`USB_PID`) | `0x822E` | board variant `pins_arduino.h` |
| Vendor HID usage page | `0xFF00`, Usage `0x01` | `USBHIDVendor` framework descriptor |
| Vendor HID report ID | `0x06` | `USBHID.h` (`HID_REPORT_ID_VENDOR`) |
| Vendor report size | 64 bytes on the wire (`0x06` Report ID + 63 data bytes) | `CustomHIDDevice` |
| Protocol payload per frame | 60 bytes (63-byte frame minus 3-byte header) | `CustomHIDDevice` |

## Composite USB Functionality

The native USB port presents a **composite device** with multiple HID interfaces plus a CDC serial interface (used for firmware logs):

| Interface | Purpose | Report / note |
|---|---|---|
| CDC (serial) | Debug logs over `Serial` (`Serial0` is UART `115200`) | not part of the protocol |
| HID Keyboard | `HID_KEY`, `TEXT` actions | report ID `0x01` |
| HID Consumer Control | `CONTROL_KEY` actions (volume, mute…) | report ID `0x04` |
| HID Mouse | `MOUSE_MOVE`, `MOUSE_CLICK` actions | report ID `0x02` |
| HID Vendor tube | **LucydDeck cmd protocol** | report ID `0x06`, see [USB-Protocol](USB-Protocol) |

A host app that only cares about config management / plugin events needs **only** the vendor HID interface. It must not open the keyboard / mouse / consumer interfaces (those are OS-consumed for local action execution).

## Firmware Build Constants (returned over the wire)

Defined in `platformio.ini` under `[common]`:

| Macro | Value | Returned by |
|---|---|---|
| `VERSION` | `vX.X.X` | `RESP_VERSION` |
| `DEVICE_NAME` | `LucydDeck` | `RESP_DEVICE_NAME` |
| `BOARD_NAME` | `WAVESHARE_ESP32_S3_TOUCH_LCD_4_3` | `RESP_BOARD_INFO` |

## Documentation Map

| Page | Contents |
|---|---|
| [**USB-Protocol**](USB-Protocol) | 64-byte report anatomy, directional opcode tables, sequence numbering, M-bit end-of-message, error codes, request/response framing |
| [**File-Transfer-Protocol**](File-Transfer-Protocol) | `CMD_FILE_START` → `CMD_FILE_CHUNK` → `CMD_FILE_END` workflow, path grammars, CRC32, atomic `.tmp` staging |
| [**Configuration-Schema**](Configuration-Schema) | Page JSON schema (3×5 grid, keys `0..14`), button properties, action-string grammar |
| [**Storage-Structure**](Storage-Structure) | SD card directory layout, `<id>.json` page naming, profile operations, boot-time provisioning |

## Quick Start (Host App Checklist)

1. Open vendor HID usage page `0xFF00`, report ID `0x06`; incoming/outgoing reports are 64 bytes.
2. Send `CMD_VERSION` with sequence `0`; expect `RESP_VERSION` + `vX.X.X` back. (Sequence doesn't matter on the very first frame.)
3. Send `CMD_PROFILES_LIST` / `CMD_IMAGE_LIST` to reconcile hashes.
4. Create profiles with `CMD_PROFILE_CREATE`, then upload page configs and icons with the file-transfer flow.
5. Navigate with `CMD_NAVIGATE` (`PAGE:<id>` / `PROFILE:<name>`), and listen for `EVT_ACTION_TRIGGERED` when the user presses buttons.
