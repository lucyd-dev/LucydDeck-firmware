# USB Protocol — Packet & Framing Specification

This page defines the byte-level protocol carried over the **vendor HID interface**. All constants are taken from [`OpCodes.hpp`](../src/usb/OpCodes.hpp), [`Errors.hpp`](../src/usb/Errors.hpp), and [`CustomHIDDevice.hpp`](../src/usb/CustomHIDDevice.hpp).

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

- The firmware clears its baseline on `ARDUINO_USB_STARTED_EVENT` and `ARDUINO_USB_RESUME_EVENT` (`UsbManager` → `CustomHIDDevice::resetSequence`), and whenever the host sends `CMD_RESET_SEQUENCE`.
- Once a frame has been accepted, the firmware accepts the next one only when `sequence == lastSequence + 1` within the 15-bit space, wrapping through `0x7FFF` to `0`. Before any frame has been accepted since a reset, any 15-bit value is accepted and becomes the baseline.
- Host frames must have **M = 0**. A frame with the M-bit set is rejected with `ERR_SEQUENCE` (`0x01`), as is any frame that is not exactly `previous + 1` (`CustomHIDDevice::handlePacket`).
- `CMD_RESET_SEQUENCE` (`0x04`) is exempt from the `previous + 1` rule. It is accepted with any 15-bit sequence, it never becomes the baseline, and it clears `lastSequence` and `sequenceInitialized` so the next frame may carry any value. A reset frame with the M-bit set is still rejected with `ERR_SEQUENCE`.

**Host recovery.** Send `CMD_RESET_SEQUENCE` after opening the device and after any `ERR_SEQUENCE`, then retry the failed command with the next sequence value. Advance the host counter for every frame sent, including the reset. Firmware that predates the command answers `ERR_UNKNOWN_COMMAND` when the frame is in sequence, and the host can carry on with its own counter. When that older firmware is already out of step it answers `ERR_SEQUENCE`, because its sequence check runs before dispatch, so the host has to ask the user to replug.

A reset does not cancel an active file transfer. If the desync happened mid-upload, send `CMD_FILE_CANCEL` before retrying the upload.

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
[0x00, 00 00, ···]  CMD_PING ----->  [0xFF, 00 00]              (ACK, heartbeat)
[0x01, 00 01, ···]  GET_DEVICE_INFO -> [0x80, 00 00, {"fw_version":"v0.1.0",...}]
[0x03, 00 02, ···]  PROFILES_LIST -> [0x82, 80 00, <chunk0>]   (M=1)
                                      [0x82, 80 01, <chunk1>]   (M=1)
                                      [0x82, 00 02, <chunk2>]   (M=0, last)
[0x40, 00 03, "gaming"] ---------->  [0xFF, 00 00]              (ACK, SET_ACTIVE_PROFILE)
```

### Host-side reassembly rule (no firmware code)

Response streams are turn-based — the host has at most **one outstanding request**. Device frames of other opcodes received mid-stream (e.g. an `EVT_ACTION_TRIGGERED` `0xA0` event) are independent events; the stream reassembler ignores them and keys on the **expected response opcode**. The device TX path is mutex-serialized, so a stream is never interleaved with another device→host stream.

## 4. Opcode Table

All opcodes come from `enum Command` in [`OpCodes.hpp`](../src/usb/OpCodes.hpp).

### Host → Device (`0x00..0x7F`)

| Opcode | Name | Payload | Response |
|---|---|---|---|
| `0x00` | `CMD_PING` | — | `RESP_ACK` |
| `0x01` | `CMD_GET_DEVICE_INFO` | — | `RESP_DEVICE_INFO` |
| `0x02` | `CMD_GET_IMAGES_LIST` | — | `RESP_IMAGES_LIST` |
| `0x03` | `CMD_GET_PROFILES_LIST` | — | `RESP_PROFILES_LIST` |
| `0x04` | `CMD_RESET_SEQUENCE` | — | `RESP_ACK` |
| `0x20` | `CMD_PROFILE_CREATE` | profile name (`validateEntityName`) | `RESP_ACK` / `RESP_ERROR` |
| `0x21` | `CMD_PROFILE_RENAME` | `old:new` (`validateName` : `validateEntityName`) | `RESP_ACK` / `RESP_ERROR` |
| `0x22` | `CMD_PROFILE_DELETE` | profile name | `RESP_ACK` / `RESP_ERROR` |
| `0x23` | `CMD_PAGE_DELETE` | `<profile>/<id>` | `RESP_ACK` / `RESP_ERROR` |
| `0x24` | `CMD_ICON_DELETE` | `<name>` | `RESP_ACK` / `RESP_ERROR` |
| `0x30` | `CMD_FILE_START` | target type + identity | `RESP_ACK` / `RESP_ERROR` |
| `0x31` | `CMD_FILE_CHUNK` | ≤ 60 B binary | `RESP_ACK` / `RESP_ERROR` |
| `0x32` | `CMD_FILE_END` | CRC32 (4 B, BE) | `RESP_ACK` / `RESP_ERROR` |
| `0x33` | `CMD_FILE_CANCEL` | — | `RESP_ACK` / `RESP_ERROR` |
| `0x40` | `CMD_SET_ACTIVE_PROFILE` | profile name | `RESP_ACK` / `RESP_ERROR` |
| `0x41` | `CMD_SET_ACTIVE_PAGE` | page id | `RESP_ACK` / `RESP_ERROR` |

`CMD_PING` is a zero-payload heartbeat; the device answers it with `RESP_ACK` immediately.

#### `CMD_FILE_START` target grammar

The payload is `[ kind (1 byte) ][ identity (string) ]`, both from `TargetType` in [`OpCodes.hpp`](../src/usb/OpCodes.hpp). The host names the entity; the firmware builds the storage path and appends the extension.

| kind | `TargetType` | identity | firmware target |
|---|---|---|---|
| `0x01` | `TARGET_PAGE` | `<profile>/<id>` | `/profiles/<profile>/<id>.json` |
| `0x02` | `TARGET_ICON` | `<name>` | `/icons/<name>.png` |

- Page identities split on the first `/`: the profile must pass `validateName` and its directory must exist; the id is decimal `0`–`255`.
- Icon identities are a flat `validateEntityName` — 1 to 64 bytes of letters, digits, space, `-` and `_`, no leading or trailing space — without an extension. An icon already on the card may be overwritten under a legacy name that only passes `validateName`.
- Full validation rules and the transfer flow are in [File-Transfer-Protocol §2](File-Transfer-Protocol#2-cmd_file_start-0x30--target-types).

#### Profile names

`CMD_PROFILE_CREATE` takes a name that must pass `Storage::validateEntityName`: 1 to 64 bytes of ASCII letters, digits, space, `-` and `_`, with no leading or trailing space. `Gaming Room` is accepted; `Gaming: Room`, `v1.2` and ` Gaming` are not.

`CMD_PROFILE_RENAME` splits `old:new` on the first `:`. The old name is a lookup of an existing directory and passes `validateName`; the new name creates a directory and passes `validateEntityName`. A rejected new name returns `ERR_INVALID_PATH` (`0x10`) and leaves the directory untouched.

`CMD_PROFILE_DELETE`, `CMD_SET_ACTIVE_PROFILE` and profile scanning keep `validateName`, so a profile created under the older rule still activates, renames away and deletes.

#### `CMD_PAGE_DELETE` and `CMD_ICON_DELETE`

Both take the identity `CMD_FILE_START` uses for that entity and remove the file it targets, answering `RESP_ACK` on success: `0x23` takes `<profile>/<id>` and removes `/profiles/<profile>/<id>.json`, `0x24` takes `<name>` and removes `/icons/<name>.png`. The page identity follows [File-Transfer-Protocol §2](File-Transfer-Protocol#2-cmd_file_start-0x30--target-types), with the page id normalized (`default/03` removes `default/3.json`). The icon name is a lookup of a file that already exists, so it only has to pass `validateName`; an icon written before the entity grammar stays deletable. Neither accepts an extension, because the firmware appends it.

Errors — nothing is written or removed on any of them:

| Code | When |
|---|---|
| `0x10` `ERR_INVALID_PATH` | profile half or icon name fails `validateName`, carries an extension, or the payload has no page half (`default`, `default/`) |
| `0x11` `ERR_INVALID_PAGE_NAME` | page half is not a decimal id `0`–`255`, including one carrying `.json` or a second slash |
| `0x12` `ERR_PROFILE_NOT_FOUND` | the profile directory is missing |
| `0x2A` `ERR_FILE_NOT_FOUND` | the page or icon file is missing |
| `0x16` `ERR_DELETE` | the unlink fails |

**Page lifecycle.** A page delete rebuilds the profile's cached page list (`ConfigManager::refreshProfile`) whether or not the unlink succeeds, so the next `CMD_GET_PROFILES_LIST` reports what is actually on the card. If the deleted page is the one on screen, the deck repaints the lowest remaining page id of the active profile, or a blank deck when the profile has none left. Deleting any other page leaves the screen untouched: `ConfigManager` records the displayed page in `loadPage`/`loadProfile`, and `isDisplayedPage` is the check that separates the two cases.

**Icon lifecycle.** `CMD_GET_IMAGES_LIST` scans `/icons/` on every request, so nothing is cached and no refresh is needed. The display does keep the decoded bitmap: an icon transfer (`CMD_FILE_END` on a `TARGET_ICON`) and a successful `CMD_ICON_DELETE` both drop the cache entry for `/icons/<name>.png` and repaint, so a new, replaced or removed icon reaches the screen without a reboot. A key whose icon file is missing renders as a key without one — background colour and label — with a `[WARN]` line naming the path.

**Page lifecycle on transfer.** A page commit refreshes the profile's page list and, when the committed page is the one on screen, re-reads it from the card and repaints (`ConfigManager::reloadDisplayedPage`). A page that no longer parses after the commit leaves the current frame up. A commit to any other page leaves the screen untouched.

### Device → Host (`0x80..0xFF`)

| Opcode | Name | Payload | Notes |
|---|---|---|---|
| `0x80` | `RESP_DEVICE_INFO` | JSON object | may be an M-bit stream |
| `0x81` | `RESP_IMAGE_LIST` | JSON array | may be an M-bit stream |
| `0x82` | `RESP_PROFILES_LIST` | JSON array | may be an M-bit stream |
| `0xA0` | `EVT_ACTION_TRIGGERED` | action string | unsolicited, never ACKed |
| `0xFE` | `RESP_ERROR` | 1-byte ErrorCode | |
| `0xFF` | `RESP_ACK` | header-only | |

`RESP_DEVICE_INFO` (`CMD_GET_DEVICE_INFO`) returns a JSON object with:
`fw_version` (firmware semver string), `protocol_version` (integer, from the `PROTOCOL_VERSION` build flag), `board` (board name string), and `free_space_kb` (SD card free space in KiB, computed at runtime).

`EVT_ACTION_TRIGGERED` is emitted when a button runs a `CMD:` action; the resolved command string is forwarded (`UsbManager::executeCmd`) with the `CMD:` prefix stripped (a `CMD:obs:scene:Scene 1` action sends `obs:scene:Scene 1`). Unknown namespaces never produce an event — the device drops them at parse time.

## 5. Acknowledgement Contract

Every host command is answered with **exactly one** device frame:

- A **data response** (`RESP_*`) for the info/list queries,
- `RESP_ACK` on success for mutating commands,
- `RESP_ERROR` + 1-byte `ErrorCode` on failure.

`CMD_RESET_SEQUENCE` is the one command answered with `RESP_ACK` while the device's sequence baseline is out of step. The device clears the baseline before sending the ACK.

Unknown-opcode handling is **directional**:

- **Host → Device:** a host-direction opcode the device does not recognize is answered with `RESP_ERROR ERR_UNKNOWN_COMMAND` (`0x02`) (`Dispatcher`).
- **Device → Host:** the device drops unrecognized content rather than forwarding it — unknown action namespaces are discarded when the page config is parsed, and the firmware only ever emits defined device-direction opcodes. Unknown actions therefore never become an `EVT_ACTION_TRIGGERED` frame for the host to deal with.

## 6. Error Codes

`RESP_ERROR` carries exactly one byte: a value from `enum ErrorCode` in [`Errors.hpp`](../src/usb/Errors.hpp) (shared by the protocol and storage layers). The table is exhaustive:

| Code | Name | Meaning |
|---|---|---|
| `0x00` | `OK` | success (not sent as an error) |
| `0x01` | `ERR_SEQUENCE` | host frame not exactly `prev+1`, or M-bit set |
| `0x02` | `ERR_UNKNOWN_COMMAND` | unknown host-direction opcode |
| `0x03` | `ERR_INVALID_DIRECTION` | host used a device-direction opcode (bit 7) |
| `0x10` | `ERR_INVALID_PATH` | a name failed `validateName` (path safety) or `validateEntityName` (new entity name), or an icon identity carried an extension |
| `0x11` | `ERR_INVALID_PAGE_NAME` | page identity not a decimal id `0`–`255` |
| `0x12` | `ERR_PROFILE_NOT_FOUND` | profile dir missing (upload, rename src, delete) |
| `0x13` | `ERR_PROFILE_EXISTS` | create or rename target conflicts |
| `0x14` | `ERR_CREATE` | `SD.mkdir` failed |
| `0x15` | `ERR_RENAME` | `SD.rename` failed |
| `0x16` | `ERR_DELETE` | delete failed (recursive directory or single file) |
| `0x20` | `ERR_TRANSFER_BUSY` | `CMD_FILE_START` while a transfer is active |
| `0x21` | `ERR_INVALID_TARGET_TYPE` | first byte of `CMD_FILE_START` is not a known target type (`0x01`/`0x02`) |
| `0x22` | `ERR_FILE_OPEN` | `.tmp` open failed |
| `0x23` | `ERR_CHUNK_TOO_LARGE` | chunk exceeds the internal cap |
| `0x24` | `ERR_FILE_NOT_OPEN` | `CMD_FILE_CHUNK` with no active transfer |
| `0x25` | `ERR_WRITE` | short write / FS error |
| `0x26` | `ERR_NO_TRANSFER` | `CMD_FILE_END`/`CMD_FILE_CANCEL` with none active |
| `0x27` | `ERR_CRC_MISSING` | `CMD_FILE_END` payload shorter than 4 bytes |
| `0x28` | `ERR_CRC` | computed CRC32 ≠ expected; staging file discarded |
| `0x29` | `ERR_FINALIZE` | rename of staging → target failed after CRC OK |
| `0x2A` | `ERR_FILE_NOT_FOUND` | delete target file missing on the SD card |
| `0x30` | `ERR_UNKNOWN_ACTION` | `CMD_SET_ACTIVE_PAGE` payload not a valid page id |
| `0x31` | `ERR_PAGE_LOAD` | page target missing / failed to load |
| `0x32` | `ERR_PROFILE_LOAD` | profile target missing / failed to load |

## 7. TX Behavior Worth Knowing

- The device TX path uses a 1000 ms lock timeout; if the USB write stalls the frame is **dropped** (no retry) (`CustomHIDDevice`).
- M-bit masks: always `seq & 0x8000` for M, `seq & 0x7FFF` for the chunk index.

## 8. Reference Implementation Files

| File | Responsibility |
|---|---|
| `src/usb/OpCodes.hpp` | opcode enum + `TargetType` + identity grammars |
| `src/usb/Errors.hpp` | `ErrorCode` enum (1-byte error table) |
| `src/usb/CustomHIDDevice.hpp/.cpp` | framing, M-bit EOM, sequence & direction enforcement, TX/RX |
| `src/usb/protocol/Dispatcher.hpp/.cpp` | opcode → handler routing, ACK/ERROR emission |
| `src/usb/protocol/handlers/*` | DeviceInfo / Profile / Delete / FileTransfer / Query handlers |
| `src/core/DeckController.hpp/.cpp` | `CMD_SET_ACTIVE_PROFILE` / `CMD_SET_ACTIVE_PAGE`, internal actions, reselect/re-render |
