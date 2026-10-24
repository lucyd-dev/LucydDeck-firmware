# File Transfer Protocol — Asset & Config Upload Workflow

Uploading files (page configs, icon images) uses a three-phase sequence of `CMD_FILE_*` commands. Every command is answered with exactly one `RESP_ACK` or `RESP_ERROR` (see [USB-Protocol §5](USB-Protocol#5-acknowledgement-contract)). Implementation lives in `Storage` (`src/hardware/Storage.*`), dispatched by `FileTransferHandler` (`src/usb/protocol/handlers/FileTransferHandler.*`).

## 1. Overview

```
 Host                              Device
─────                              ──────
 CMD_FILE_START  (kind + identity) ──►  validate identity, open <target>.tmp, reset CRC32
 RESP_ACK / RESP_ERROR               ◄──
 CMD_FILE_CHUNK  (≤60 bytes)  ─────────►  append bytes, update running CRC32
 RESP_ACK / RESP_ERROR               ◄──
 ... (repeat for each chunk, seq + 1 each) ...
 CMD_FILE_END  (CRC32, 4B BE)  ────►  compare CRC, atomic rename .tmp → target
 RESP_ACK / RESP_ERROR               ◄──
```

The firmware **never** replaces the destination in-place: it streams into a `<target>.tmp` staging file and only atomically renames it over the target **after** the CRC32 check passes. A failed or aborted transfer leaves the last-good file intact. `CMD_FILE_CANCEL` aborts an in-flight transfer at any time (answer: `RESP_ACK`; with no active transfer: `ERR_NO_TRANSFER`).

## 2. `CMD_FILE_START` (0x30) — Target Types

The payload layout is `[ kind (1 byte) ][ identity (string) ]`. `TargetType` lives in [`OpCodes.hpp`](../src/usb/OpCodes.hpp). The host addresses the entity; the firmware builds the storage path and appends the extension.

| kind | Constant | Identity | Firmware target |
|---|---|---|---|
| `0x01` | `TARGET_PAGE` | `"<profile>/<id>"` | `/profiles/<profile>/<id>.json` |
| `0x02` | `TARGET_ICON` | `"<name>"` | `/icons/<name>.png` |

No extension is accepted from the host. A page identity of `gaming/3.json` and an icon identity of `github.png` are rejected rather than turned into `3.json.json` or `github.png.png`.

### Page identity (`TARGET_PAGE`)

- Split on the **first** `/` into profile and id.
- The profile half must pass `Storage::validateName` (non-empty, not `.`/`..`, no `/` or `\`, no control characters) — otherwise `ERR_INVALID_PATH` (`0x10`).
- The id half must be decimal **`0`–`255`**: digits only, at most three. Values are normalized, so `03` and `3` name the same page and commit to `<id>.json`. `.json` or a second slash yields `ERR_INVALID_PAGE_NAME` (`0x11`).
- The profile directory **must already exist** (create it first with `CMD_PROFILE_CREATE`). A missing profile yields `ERR_PROFILE_NOT_FOUND` (`0x12`) **before** any `.tmp` file is opened; there is no auto-create.
- On success the file is committed to `/profiles/<profile>/<id>.json` and the profile's page list is refreshed, so the new page is immediately visible in `CMD_GET_PROFILES_LIST` and navigable without a reboot (`ConfigManager::refreshProfile`).

### Icon identity (`TARGET_ICON`)

- Flat `validateName` on the whole identity — target `/icons/<name>.png`.
- The identity must not carry an extension: a trailing `.png` or `.json` (case-insensitive) yields `ERR_INVALID_PATH` (`0x10`).

## 3. `CMD_FILE_CHUNK` (0x31) — Streaming

- Payload is raw binary, **up to 60 bytes** per frame (the wire max).
- The firmware appends the chunk and feeds it into the running CRC-32 accumulator.
- Errors: `ERR_TRANSFER_BUSY` can only occur on `START`; with no active transfer → `ERR_FILE_NOT_OPEN` (`0x24`); chunk above the internal cap → `ERR_CHUNK_TOO_LARGE` (`0x23`, defensive — 60 > it is impossible on the wire).

## 4. `CMD_FILE_END` (0x32) — CRC32 Finalization

- Payload is exactly **4 bytes, big-endian**, the CRC-32 of **all bytes received since `CMD_FILE_START`**.
- `< 4` bytes → `ERR_CRC_MISSING` (`0x27`) and the transfer is cancelled (`Storage::cancelFileTransfer`).
- CRC mismatch (computed ≠ expected) → `ERR_CRC` (`0x28`) and the staging file is deleted.
- CRC match → staging file is renamed over the target and `RESP_ACK` is returned (`Storage::finishFile`).

**CRC-32 contract:** the standard IEEE/zlib CRC-32 (polynomial `0x04C11DB7`, reflected, init `0xFFFFFFFF`, final XOR `0xFFFFFFFF`) — the same check value used by PNG/zlib. It is computed by the `bakercp/CRC32` library with its default parameters, matching `Storage::calculateFileCRC` which is what the firmware uses to produce the `CMD_GET_IMAGES_LIST` / `CMD_GET_PROFILES_LIST` hashes. A host that uploads files walks the same algorithm over the exact byte sequence it sends.

```
expected CRC32 (big-endian) = CRC32(chunk0 bytes ++ chunk1 bytes ++ ... ++ chunkN bytes)
```

## 5. Atomic Commit & Failure Modes

| Condition | Error code | Behavior |
|---|---|---|
| CRC mismatch | `ERR_CRC` | delete `.tmp`, keep old target |
| Final rename fails | `ERR_FINALIZE` | delete `.tmp`, keep old target |
| Write error mid-stream | `ERR_WRITE` | close + delete `.tmp` |
| Inactivity > `FILE_TRANSFER_TIMEOUT` (1000 ms) | — (silent) | close + delete `.tmp`; **no error frame is sent** (`Storage::loop`) |
| `CMD_FILE_END` with no active transfer | `ERR_NO_TRANSFER` | |
| `CMD_FILE_START` while a transfer is active | `ERR_TRANSFER_BUSY` | |

## 6. Worked Example (page upload)

Host sends frames with incrementing sequence (`…` = zero padding). The profile `gaming` already exists:

```
seq=10  [0x30, 00 0A, 0x01, 'g','a','m','i','n','g','/','3', 00 ... 00]                          CMD_FILE_START  (page identity)
        <— RESP_ACK
seq=11  [0x31, 00 0B, <60 bytes of JSON>]                                                        CMD_FILE_CHUNK
        <— RESP_ACK
seq=12  [0x31, 00 0C, <remaining N bytes of JSON>]                                               CMD_FILE_CHUNK
        <— RESP_ACK
seq=13  [0x32, 00 0D, 0x12 0x34 0x56 0x78]                                                       CMD_FILE_END (CRC BE)
        <— RESP_ACK   (renamed /profiles/gaming/3.json in place; PROFILES_LIST now shows page id 3)
```

## 7. Upload-Side Best Practices (Host App)

1. Query `CMD_GET_IMAGES_LIST` / `CMD_GET_PROFILES_LIST` first and compare `hash` values; skip files that already match.
2. Create the target profile with `CMD_PROFILE_CREATE` (or rename an existing one) before uploading pages to it.
3. Send chunks as large as possible (60 bytes) to reduce round-trips.
4. Keep inter-chunk spacing well under the 1 s inactivity timeout; `RESP_ACK` for each chunk is the pacing signal.
5. Compute the CRC over the concatenated bytes in the exact order sent, and send it big-endian.
6. `CMD_GET_PROFILES_LIST` and `CMD_GET_IMAGES_LIST` return filenames with extensions (`3.json`, `github.png`). Strip `.json` / `.png` to get the identity for `CMD_FILE_START`.
