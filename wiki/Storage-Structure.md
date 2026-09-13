# Storage Structure — SD Card Layout

Configuration and asset files live on the device's microSD card, accessed through the Arduino `SD` (FAT) filesystem. Constants are in [`Storage.hpp`](../src/hardware/Storage.hpp); boot provisioning and enumeration are in [`ConfigManager.hpp`](../src/config/ConfigManager.hpp).

## 1. Required Directory Hierarchy

```
/                          (SD card root)
├── icons/                 created at startup (DisplayManager)
│   ├── <name>.png         icon images referenced by imageName
│   └── ...
└── profiles/              created at startup (ConfigManager::begin)
    ├── default/           default profile, auto-created if none exist
    │   ├── <id>.json      one JSON file per page
    │   └── ...
    └── <other-profile>/
        └── *.json
```

| Path constant | Value | Purpose |
|---|---|---|
| `IMAGE_DIR` | `/icons/` | icon PNGs |
| `PROFILES_DIR` | `/profiles/` | profile directories, each holding page JSONs |

## 2. Boot-Time Provisioning (`ConfigManager::begin`)

1. Create `/profiles/` if missing.
2. Scan `/profiles/` for profile directories.
3. If **no** profiles exist, create a `default` profile directory.
4. Select a profile via `ConfigManager::ensureProfileSelected` (prefers `default`, else the first profile found) and load its first page.
5. `/icons/` is created independently by `DisplayManager::initializeLVGL`.

## 3. Profile & Page Enumeration

- **Profiles** are all **directories** directly under `/profiles/`, enumerated in SD listing order.
- **Pages** are all files inside a profile directory whose name matches **`^[0-9]+\.json$`**.
- **Page ID = the leading digits of the filename** (decimal, unpadded: `0.json`, `1.json`, …, `255.json`). `ConfigManager::scanProfileDir` parses the ID with `parsePageId` and inserts `profiles[profile][id] = filename`, skipping a duplicate ID (first-sorted wins, deterministic). Non-matching files are logged and skipped — they produce no page.

> Page IDs are **filename-encoded**, not enumeration-order-encoded. `CMD_GET_PROFILES_LIST` returns the parsed `id` alongside the `filename`; a host app should always reconcile against that response.

## 4. Naming Conventions & Format Rules

Enforced by `Storage::validateName`:

- Names must be non-empty.
- Must **not** be `.` or `..`.
- Must **not** contain `/` or `\`.
- Must not contain ASCII control characters (`< 0x20`).

| Item | Supported formats | Naming rule |
|---|---|---|
| Page config | `.json` | stored under `/profiles/<profile>/`; only files matching `^[0-9]+\.json$` are pages |
| Icons | `.png` | `imageName` in JSON is the **base name without extension**; firmware looks up `/icons/<imageName>.png` |

The firmware does **not** validate `.png`/`.json` content — it loads by path. Icon lists and profile lists returned over the wire include the full filename (with extension).

## 5. Wire Commands That Touch the SD Card

| Command | Effect |
|---|---|
| `CMD_PROFILE_CREATE` | `SD.mkdir(/profiles/<name>)`; pre-checks existence → `ERR_PROFILE_EXISTS`, failure → `ERR_CREATE` |
| `CMD_PROFILE_RENAME` | `SD.rename(/profiles/<old>, /profiles/<new>)`; missing src → `ERR_PROFILE_NOT_FOUND`, target exists → `ERR_PROFILE_EXISTS`, failure → `ERR_RENAME`; if the renamed profile is active, current selection follows |
| `CMD_PROFILE_DELETE` | **recursive** delete (`Storage::deleteDirRecursive` removes files and subdirs, then `SD.rmdir`); missing → `ERR_PROFILE_NOT_FOUND`, failure → `ERR_DELETE`; if the deleted profile is active, `ConfigManager::ensureProfileSelected` reselects and the display re-renders |
| `CMD_FILE_START` | stages to `/profiles/<profile>/<page.json>` (page) or `/icons/<name>` (icon) (see [File-Transfer-Protocol](File-Transfer-Protocol)) |
| `CMD_FILE_END` | atomic rename `.tmp` → target; on a page commit the profile's page list is rebuilt (`ConfigManager::refreshProfile`) |

All profile ops build paths as `PROFILES_DIR + name`; no host string ever reaches `SD.*` unvalidated.

## 6. Hash / Reconciliation (`CMD_GET_IMAGES_LIST`, `CMD_GET_PROFILES_LIST`)

- `/icons/` contents → `[{"filename": "<name>", "hash": <CRC32>}]` (`QueryHandler::sendImageList`). **All** files in `/icons/` are listed, regardless of extension.
- Profiles → `[{"name": "<profile>", "pages": [{"id": <n>, "filename": "<page.json>", "hash": <CRC32>}]}]` (`ProfileHandler::sendProfilesList`).
- Hashes are **CRC-32** of the full file, computed by `Storage::calculateFileCRC` (same algorithm as file-transfer finalization — see [File-Transfer-Protocol §4](File-Transfer-Protocol#4-cmd_file_end-0x32--crc32-finalization)). Zero (`0`) means the file could not be opened.

These responses let a host app perform delta syncs before uploading.

## 7. Reference Implementation Files

| File | Responsibility |
|---|---|
| `src/hardware/Storage.hpp/.cpp` | path constants, `validateName`, dir/file operations, transfer staging |
| `src/config/ConfigManager.hpp/.cpp` | profile/page scanning (`scanProfileDir`, `parsePageId`), JSON parsing, boot provisioning, `refreshProfile`/`ensureProfileSelected` |
| `src/display/DisplayManager.hpp/.cpp` | icon path resolution (`S:<path>.png`) |
