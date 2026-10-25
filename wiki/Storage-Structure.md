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

## 2. Boot-Time Provisioning

At mount (`Storage::begin`), once the card answers and before anything reads or renders, `Storage::sweepStaleTransferFiles` settles the files a transfer interrupted by a crash, an unplug or a power loss left behind (see [File-Transfer-Protocol §5](File-Transfer-Protocol#5-atomic-commit--failure-modes)). It walks `/icons/` and every `/profiles/<profile>/`, logs the number of files it removed or restored, and returns early when the card is absent. This runs before `ConfigManager::begin`, so a restored `<id>.json` is enumerated at boot.

Then `ConfigManager::begin` provisions and loads:

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

Two grammars live in `Storage`, and which one applies depends on whether a name is being **read** or **created**.

**Path safety — `Storage::validateName`.** For reads, lookups and names that already exist on the card:

- Names must be non-empty.
- Must **not** be `.` or `..`.
- Must **not** contain `/` or `\`.
- Must not contain ASCII control characters (`< 0x20`).

Everything else passes, so a profile, page or icon written before the entity grammar still loads, lists and deletes.

**Entity names — `Storage::validateEntityName`.** For names that create a storage entity:

- 1 to 64 bytes.
- ASCII letters, digits, space, `-` and `_` only — no `:`, `*`, `?`, `"`, `<`, `>`, `|`, no dot, no separator, no non-ASCII byte.
- No leading or trailing space.

A name accepted here is a legal FAT long name and can never carry path syntax, so the entity always stays inside `PROFILES_DIR` or `IMAGE_DIR`.

| Item | Supported formats | Naming rule |
|---|---|---|
| Profile directory | — | `validateEntityName` on `CMD_PROFILE_CREATE` and as the rename target; `validateName` for delete, activation, scanning and the profile half of a page identity |
| Page config | `.json` | stored under `/profiles/<profile>/`; only files matching `^[0-9]+\.json$` are pages |
| Icons | `.png` | an uploaded name must pass `validateEntityName` (an icon already on the card may be overwritten under a legacy `validateName` name); delete, listing and the `imageName` lookup use `validateName`. `imageName` in JSON is the **base name without extension**, looked up as `/icons/<imageName>.png` |

The firmware does **not** validate `.png`/`.json` content — it loads by path. Icon lists and profile lists returned over the wire include the full filename (with extension).

## 5. Wire Commands That Touch the SD Card

| Command | Effect |
|---|---|
| `CMD_PROFILE_CREATE` | `SD.mkdir(/profiles/<name>)`; a name that fails `validateEntityName` → `ERR_INVALID_PATH` before the card is touched; pre-checks existence → `ERR_PROFILE_EXISTS`, failure → `ERR_CREATE` |
| `CMD_PROFILE_RENAME` | `SD.rename(/profiles/<old>, /profiles/<new>)`; old name `validateName`, new name `validateEntityName` (else `ERR_INVALID_PATH`, directory untouched); missing src → `ERR_PROFILE_NOT_FOUND`, target exists → `ERR_PROFILE_EXISTS`, failure → `ERR_RENAME`; if the renamed profile is active, current selection follows |
| `CMD_PROFILE_DELETE` | **recursive** delete (`Storage::deleteDirRecursive` removes files and subdirs, then `SD.rmdir`); missing → `ERR_PROFILE_NOT_FOUND`, failure → `ERR_DELETE`; if the deleted profile is active, `ConfigManager::ensureProfileSelected` reselects and the display re-renders |
| `CMD_PAGE_DELETE` | unlink `/profiles/<profile>/<id>.json`; missing profile dir → `ERR_PROFILE_NOT_FOUND`, missing file → `ERR_FILE_NOT_FOUND`, failure → `ERR_DELETE`; the profile's page list is rebuilt (`ConfigManager::refreshProfile`); if the displayed page goes, the lowest remaining page repaints, or the deck blanks |
| `CMD_ICON_DELETE` | unlink `/icons/<name>.png`; the name only has to pass `validateName`, so an icon from the older grammar can still be removed; missing file → `ERR_FILE_NOT_FOUND`, failure → `ERR_DELETE`; the image list is scanned per request, and the displayed page drops the decoded bitmap and repaints the key without it |
| `CMD_FILE_START` | stages to `/profiles/<profile>/<id>.json` (page) or `/icons/<name>.png` (icon) (see [File-Transfer-Protocol](File-Transfer-Protocol)) |
| `CMD_FILE_END` | atomic rename `.tmp` → target; on a page commit the profile's page list is rebuilt (`ConfigManager::refreshProfile`) and a commit to the displayed page re-reads and repaints it; an icon commit repaints the deck so the key shows the file as committed |

All profile ops build paths as `PROFILES_DIR + name`; no host string ever reaches `SD.*` unvalidated, and a name that creates a directory or file must also pass `validateEntityName`.

## 6. Hash / Reconciliation (`CMD_GET_IMAGES_LIST`, `CMD_GET_PROFILES_LIST`)

- `/icons/` contents → `[{"filename": "<name>", "hash": <CRC32>}]` (`QueryHandler::sendImageList`). **All** files in `/icons/` are listed, regardless of extension.
- Profiles → `[{"name": "<profile>", "pages": [{"id": <n>, "filename": "<page.json>", "hash": <CRC32>}]}]` (`ProfileHandler::sendProfilesList`).
- Hashes are **CRC-32** of the full file, computed by `Storage::calculateFileCRC` (same algorithm as file-transfer finalization — see [File-Transfer-Protocol §4](File-Transfer-Protocol#4-cmd_file_end-0x32--crc32-finalization)). Zero (`0`) means the file could not be opened.

These responses let a host app perform delta syncs before uploading.

## 7. Reference Implementation Files

| File | Responsibility |
|---|---|
| `src/hardware/Storage.hpp/.cpp` | path constants, `validateName` / `validateEntityName`, dir/file operations, transfer staging |
| `src/config/ConfigManager.hpp/.cpp` | profile/page scanning (`scanProfileDir`, `parsePageId`), JSON parsing, boot provisioning, `refreshProfile`/`ensureProfileSelected` |
| `src/display/DisplayManager.hpp/.cpp` | icon path resolution (`S:<path>.png`) |
