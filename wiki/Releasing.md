# Releasing — From a Merged Change to an Installable Release

A LucydDeck firmware release is a Git tag, an app image and a `firmware.json` manifest, published together by one workflow. The deck's companion app reads the manifest from the newest stable release, verifies the image against it and installs the image over USB, so a release is "installable" only when all three exist and agree.

This page is the walk-through. Every field, limit and refusal the manifest carries lives in [Firmware-Manifest](Firmware-Manifest).

## 1. What a Release Contains

| Asset | Produced by | Named |
|---|---|---|
| App image | `pio run -e release` → `.pio/build/release/firmware.bin` | `LucydDeck-firmware-v<version>.bin` |
| Manifest | [`scripts/firmware_manifest.py`](../scripts/firmware_manifest.py) | `firmware.json` |
| Release notes | release-please, from the merged commits | the release body |

`version` comes from [`version.txt`](../version.txt). The same file is compiled into the firmware as `FW_VERSION` by [`version_script.py`](../version_script.py) and reported as `fw_version`, so the version the app sees on the wire and the version in the manifest are the same string (with and without the `v`).

## 2. Before the First Manifest Release

Two changes have to be on `main` together:

- `CMD_ENTER_BOOTLOADER` (`0x05`), so the running firmware can hand the deck to the ROM loader.
- The manifest step in the release workflow, described in §5.

**Do not cut a release between the two.** The first release that publishes `firmware.json` is the first release the app can install, and it must also carry the bootloader command — otherwise the app offers an update it cannot hand off. A release cut in between would also be the newest release with no manifest to fetch, so the app's update check would fail until a newer release existed. That release's notes name it as the first version with in-app updates; the workflow adds that paragraph by itself, once.

A deck that shipped before that release stays on it until it is flashed **once** from the CH343 port with PlatformIO (see the [README](../README.md)). After that flash, the app owns updates and the CH343 port is no longer needed.

## 3. Land a Change

1. Merge the change to `main` with a [Conventional Commit](https://www.conventionalcommits.org/) title.
2. The type decides what the release notes show: `feat` and `fix` and `perf` and `revert` appear, `docs`, `refactor`, `ci`, `build`, `test`, `style` and `chore` are hidden (see [`release-please-config.json`](../release-please-config.json)).
3. If the change touched the wire, the wiki page was updated in the same change. The release does not touch `protocol_version`; that waits for 1.0.0.

## 4. Merge the Release Pull Request

release-please opens a pull request on the next push to `main`, titled `chore(main): release X.Y.Z`. Merging it:

1. Bumps [`version.txt`](../version.txt) and [`.release-please-manifest.json`](../.release-please-manifest.json).
2. Writes the new section into [`CHANGELOG.md`](../CHANGELOG.md).
3. Creates the tag `vX.Y.Z` and the GitHub release.

`bump-minor-pre-major` is on, so pre-1.0.0 a feature moves the minor version (`0.3.x` → `0.4.0`) and a fix moves the patch version. Do not create the tag or the release by hand: the workflow keys off the release that release-please created.

## 5. Watch the Workflow

Merging the release pull request pushes to `main`, which runs [`.github/workflows/release-please.yml`](../.github/workflows/release-please.yml):

| Step | What it does |
|---|---|
| Build | `pio run -e release` |
| Prepare release assets | copies the image to `LucydDeck-firmware-<tag>.bin`, then runs the manifest script, which writes `firmware.json` and validates it |
| Detect the first manifest-bearing release | looks for `firmware.json` on any earlier release |
| Upload | uploads both files to the release |
| Note the first in-app-update release | on the first manifest-bearing release only, appends the in-app-update paragraph to the notes |

The build and the manifest are gated: a wrong-chip image, an image over the 8064 KiB app slot, a board whose partition table is no longer `max_app_8MB`, a tag that disagrees with `version.txt`, or a manifest field that disagrees with the image fails the job before upload. The refusals are listed in [Firmware-Manifest §6](Firmware-Manifest#6-refusals).

Between release-please creating the release and the upload step finishing, `releases/latest` points at a release with no `firmware.json` yet, so an update check in that window finds nothing. It settles as soon as the job completes; this is why the assets go up in one step, image first and manifest last.

If the job fails **after** the release was created, the tag exists with no assets and pushing again will not re-create the release. Fix the cause, then re-run the failed jobs of that workflow run: the steps re-copy, re-write and re-upload with `--clobber`.

## 6. Verify the Release

Check the two assets and the manifest the app will actually fetch:

```bash
gh release view v0.4.0 --json assets --jq '.assets[].name'
curl -fsSL https://github.com/lucyd-dev/LucydDeck-firmware/releases/latest/download/firmware.json
```

Then validate the published manifest against the published image:

```bash
curl -fsLO https://github.com/lucyd-dev/LucydDeck-firmware/releases/latest/download/firmware.json
curl -fsLO https://github.com/lucyd-dev/LucydDeck-firmware/releases/latest/download/LucydDeck-firmware-v0.4.0.bin

python3 scripts/firmware_manifest.py \
  --bin LucydDeck-firmware-v0.4.0.bin \
  --tag v0.4.0 \
  --check firmware.json
```

The checklist:

- [ ] `firmware.json` and `LucydDeck-firmware-v<tag>.bin` are both assets of the release.
- [ ] `curl` on `releases/latest/download/firmware.json` returns the manifest, not a 404 — this is the URL the app uses.
- [ ] `boards` has one key, exactly `WAVESHARE_ESP32_S3_TOUCH_LCD_4_3` — the string the deck reports in `RESP_DEVICE_INFO.board`.
- [ ] `version` matches the tag and `fw_version`.
- [ ] `--check` accepts the published pair.
- [ ] `size` is at most 8064 KiB, `address` is `65536`, `layout` is `max_app_8MB`.
- [ ] On the first manifest-bearing release, the notes say it is the first version with in-app updates.

## 7. Test an In-App Update

The app installs only a **newer** manifest `version` than the running `fw_version`, so testing needs one of:

- **A newer release.** After the first manifest-bearing release, cut the next one and let the app find it. This tests the path a deck owner takes.
- **A local build.** Flash a local image from the app's Settings → Firmware ([host ticket](https://github.com/lucyd-dev/LucydDeck/issues/20)) on a deck that already has the bootloader command. No release is involved.

Do not test against a firmware older than the bootstrap image: it answers `ERR_UNKNOWN_COMMAND` to `CMD_ENTER_BOOTLOADER`, and the app reports that the deck is too old rather than updating it. The full hardware pass — update, version re-read, profiles/pages/icons intact, a second check offering nothing, a cable pull mid-write — is in the host repository's ticket for the update flow.

## 8. Try It Locally

Build the release image and write the manifest into a scratch directory, before any release exists:

```bash
pio run -e release
python3 scripts/firmware_manifest.py \
  --bin .pio/build/release/firmware.bin \
  --out /tmp/firmware.json
cat /tmp/firmware.json
```

The version and tag come from `version.txt`; `--version 0.4.0 --tag v0.4.0` writes the manifest for a version the checkout does not carry yet, and the two must agree either way. The script prints the board key, asset, size, checksum, chip, address and layout it wrote, and refuses anything the workflow would refuse. `--repo` overrides the owner/name used in `releaseUrl`; CI takes it from `GITHUB_REPOSITORY`.

## 9. Related

- [Firmware-Manifest](Firmware-Manifest) — every field, the 8064 KiB limit and every refusal, on both sides.
- [USB-Protocol](USB-Protocol) — `RESP_DEVICE_INFO`, the board and version strings a release is matched against.
- [Home](Home) — documentation map.
