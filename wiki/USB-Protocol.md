# USB Protocol — Packet & Framing Specification (v2)

This page defines the byte-level protocol carried over the **vendor HID interface**. All constants are taken from [`Commands.hpp`](../src/usb/Commands.hpp), [`Errors.hpp`](../src/usb/Errors.hpp), and [`CustomHIDDevice.hpp`](../src/usb/CustomHIDDevice.hpp).

## 1. HID Report Anatomy

The vendor HID interface declares a single **64-byte report** (Report ID + 63 data bytes). The logical frame the firmware processes is **63 bytes**: a 3-byte header plus up to a **60-byte payload**.

```
USB HID report (64 bytes on the wire)
┌─────────┬───────────┬──────────────┬──────────────────────────────────────────┐
│ Byte 0  │ Byte 1    │ Byte 2 .. 3  │ Bytes 4 .. 63                            │
├─────────┼───────────┼──────────────┼──────────────────────────────────────────┤
│ Report  │ Command   │ Sequence     │ Payload                                  │
│ ID 0x06 │ (opcode)  │ (16-bit BE)  │ (≤ 60 bytes, host zero-pads)             │
│ ─────── ┼───────────┼──────────────┼──────────────────────────────────────────┤
│ (adds 1 │           63-byte application frame (BUFFER_SIZE = 63)              │
└─────────┴───────────┴──────────────┴──────────────────────────────────────────┘

Offsets within the 63-byte frame:
  [0]  Command / opcode
  [1]  Sequence High byte  (MSB)
  [2]  Sequence Low byte   (LSB)
  [3..62]  Payload (0..60 bytes)
```

## 2. Direction & Framing Rules

Opcode direction is encoded in **bit 7** of the command byte:

- **Host → Device** opcodes: `0x00..0x7F` (bit 7 clear).
- **Device → Host** opcodes: `0x80..0xFF` (bit 7 set).

The firmware rejects a host frame whose command byte has bit 7 set with `ERR_INVALID_DIRECTION` (`0x03`).

**Zero-padding.** The USB stack delivers each report to the firmware at its full 63-byte length, and the firmware computes the payload length from that fixed value. The host **must** fill unused payload bytes with `0x00`.

## 3. Sequence Numbering

Sequence counters are **per-direction** and independent. The 16-bit BE sequence word reserves the **MSB (mask `0x8000`, bit 7 of byte 1) as the M-bit**.

### Host → Device

- Resets to `0` on connect / `ARDUINO_USB_STARTED_EVENT` / `ARDUINO_USB_RESUME_EVENT` (`UsbManager` → `CustomHIDDevice::resetSequence`).
- The firmware accepts a packet when `sequence == lastSequence + 1`, **except** the very first packet after a reset (when `lastSequence == 0`, any value is accepted and becomes the new baseline).
- Host frames must have **M = 0**. A frame with the M-bit set is rejected with `ERR_SEQUENCE` (`0x01`), as is any frame that is not exactly `previous + 1` (`CustomHIDDevice::handlePacket`).

### Device → Host (M-bit end-of-message)

Multi-frame responses carry the M-bit while more frames follow. Chunk `k` of `n`:

```
seq = k | (k < n - 1 ? 0x8000 : 0)
```

- M (`0x8000`) is set on every frame **except the final one**.
- A single-frame response or event uses `seq = 0x0000`.
- There is **no** "short tail" heuristic: end-of-message is identified **only** by the M-bit, so a response whose length is an exact multiple of 60 is final enough to detect.

```
Host → Device                       Device → Host
─────────────────────               ─────────────────────
[0x01, 00 00, ···]  CMD_VERSION --> [0x81, 00 00, "v0.1.0"]
[0x05, 00 01, ···]  PROFILES_LIST -> [0x85, 80 00, <chunk0>]   (M=1)
                                      [0x85, 80 01, <chunk1>]   (M=1)
                                      [0x85, 00 02, <chunk2>]   (M=0, last)
[0x40, 00 02, "PAGE:2"] ---------->  [0xFF, 00 00]              (ACK)
```

### Host-side reassembly rule (no firmware code)

Response streams are turn-based — the host has at most **one outstanding request**. Device frames of other opcodes received mid-stream (e.g. an `EVT_ACTION_TRIGGERED` `0xA0` event) are independent events; the stream reassembler ignores them and keys on the **expected response opcode**. The device TX path is mutex-serialized, so a stream is never interleaved with another device→host stream.

## 4. Opcode Table

All opcodes come from `enum Command` in [`Commands.hpp`](../src/usb/Commands.hpp).

### Host → Device (`0x00..0x7F`)

| Opcode | Name | Payload | Response |
|---|---|---|---|
| `0x01` | `CMD_VERSION` | — | `RESP_VERSION` |
| `0x02` | `CMD_DEVICE_NAME` | — | `RESP_DEVICE_NAME` |
| `0x03` | `CMD_BOARD_INFO` | — | `RESP_BOARD_INFO` |
| `0x04` | `CMD_IMAGE_LIST` | — | `RESP_IMAGE_LIST` |
| `0x05` | `CMD_PROFILES_LIST` | — | `RESP_PROFILES_LIST` |
| `0x20` | `CMD_PROFILE_CREATE` | profile name | `RESP_ACK` / `RESP_ERROR` |
| `0x21` | `CMD_PROFILE_RENAME` | `old:new` | `RESP_ACK` / `RESP_ERROR` |
| `0x22` | `CMD_PROFILE_DELETE` | profile name | `RESP_ACK` / `RESP_ERROR` |
| `0x30` | `CMD_FILE_START` | pathType + path | `RESP_ACK` / `RESP_ERROR` |
| `0x31` | `CMD_FILE_CHUNK` | ≤ 60 B binary | `RESP_ACK` / `RESP_ERROR` |
| `0x32` | `CMD_FILE_END` | CRC32 (4 B, BE) | `RESP_ACK` / `RESP_ERROR` |
| `0x33` | `CMD_FILE_CANCEL` | — | `RESP_ACK` / `RESP_ERROR` |
| `0x40` | `CMD_NAVIGATE` | `PAGE:<id>` or `PROFILE:<name>` | `RESP_ACK` / `RESP_ERROR` |

### Device → Host (`0x80..0xFF`)

| Opcode | Name | Payload | Notes |
|---|---|---|---|
| `0x81` | `RESP_VERSION` | version string | |
| `0x82` | `RESP_DEVICE_NAME` | name string | |
| `0x83` | `RESP_BOARD_INFO` | board string | |
| `0x84` | `RESP_IMAGE_LIST` | JSON array | may be an M-bit stream |
| `0x85` | `RESP_PROFILES_LIST` | JSON array | may be an M-bit stream |
| `0xA0` | `EVT_ACTION_TRIGGERED` | action string | unsolicited, never ACKed |
| `0xFE` | `RESP_ERROR` | 1-byte ErrorCode | |
| `0xFF` | `RESP_ACK` | header-only | |

`EVT_ACTION_TRIGGERED` is emitted when a button runs a `CMD:` / plugin action; the action string is forwarded verbatim (`UsbManager::executeCmd`).

## 5. Acknowledgement Contract

Every host command is answered with **exactly one** device frame:

- A **data response** (`RESP_*`) for the info/list queries,
- `RESP_ACK` on success for mutating commands,
- `RESP_ERROR` + 1-byte `ErrorCode` on failure.

An **unknown** host-direction opcode is answered with `RESP_ERROR ERR_UNKNOWN_COMMAND` (`0x02`) (`Dispatcher`).

## 6. Error Codes

`RESP_ERROR` carries exactly one byte: a value from `enum ErrorCode` in [`Errors.hpp`](../src/usb/Errors.hpp) (shared by the protocol and storage layers). The table is exhaustive:

| Code | Name | Meaning |
|---|---|---|
| `0x00` | `OK` | success (not sent as an error) |
| `0x01` | `ERR_SEQUENCE` | host frame not exactly `prev+1`, or M-bit set |
| `0x02` | `ERR_UNKNOWN_COMMAND` | unknown host-direction opcode |
| `0x03` | `ERR_INVALID_DIRECTION` | host used a device-direction opcode (bit 7) |
| `0x10` | `ERR_INVALID_PATH` | name failed `validateName` |
| `0x11` | `ERR_INVALID_PAGE_NAME` | page target not `<id>.json` |
| `0x12` | `ERR_PROFILE_NOT_FOUND` | profile dir missing (upload, rename src, delete) |
| `0x13` | `ERR_PROFILE_EXISTS` | create or rename target conflicts |
| `0x14` | `ERR_CREATE` | `SD.mkdir` failed |
| `0x15` | `ERR_RENAME` | `SD.rename` failed |
| `0x16` | `ERR_DELETE` | recursive delete failed |
| `0x20` | `ERR_TRANSFER_BUSY` | `CMD_FILE_START` while a transfer is active |
| `0x21` | `ERR_INVALID_PATH_TYPE` | first byte of `CMD_FILE_START` is not `0x01`/`0x02` |
| `0x22` | `ERR_FILE_OPEN` | `.tmp` open failed |
| `0x23` | `ERR_CHUNK_TOO_LARGE` | chunk exceeds the internal cap |
| `0x24` | `ERR_FILE_NOT_OPEN` | `CMD_FILE_CHUNK` with no active transfer |
| `0x25` | `ERR_WRITE` | short write / FS error |
| `0x26` | `ERR_NO_TRANSFER` | `CMD_FILE_END`/`CMD_FILE_CANCEL` with none active |
| `0x27` | `ERR_CRC_MISSING` | `CMD_FILE_END` payload shorter than 4 bytes |
| `0x28` | `ERR_CRC` | computed CRC32 ≠ expected; staging file discarded |
| `0x29` | `ERR_FINALIZE` | rename of staging → target failed after CRC OK |
| `0x30` | `ERR_UNKNOWN_ACTION` | `CMD_NAVIGATE` payload malformed / not `PAGE`/`PROFILE` |
| `0x31` | `ERR_PAGE_LOAD` | `PAGE:<id>` target missing or unparseable |
| `0x32` | `ERR_PROFILE_LOAD` | `PROFILE:<name>` target missing or empty |

## 7. TX Behavior Worth Knowing

- The device TX path uses a 1000 ms lock timeout; if the USB write stalls the frame is **dropped** (no retry) (`CustomHIDDevice`).
- M-bit masks: always `seq & 0x8000` for M, `seq & 0x7FFF` for the chunk index.

## 8. Reference Implementation Files

| File | Responsibility |
|---|---|
| `src/usb/Commands.hpp` | opcode enum + `PathType` + payload grammars |
| `src/usb/Errors.hpp` | `ErrorCode` enum (1-byte error table) |
| `src/usb/CustomHIDDevice.hpp/.cpp` | framing, M-bit EOM, sequence & direction enforcement, TX/RX |
| `src/usb/protocol/Dispatcher.hpp/.cpp` | opcode → handler routing, ACK/ERROR emission |
| `src/usb/protocol/handlers/*` | DeviceInfo / Profile / FileTransfer / Query handlers |
| `src/core/DeckController.hpp/.cpp` | `CMD_NAVIGATE`, internal actions, reselect/re-render |
