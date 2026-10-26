#!/usr/bin/env python3
"""Write or check the board-keyed firmware manifest published with a release.

The manifest is the only metadata the LucydDeck app needs to install a release:
the image asset, its byte size and SHA-256, and where in flash it belongs.
`wiki/Firmware-Manifest.md` documents every field, limit and refusal;
`wiki/Releasing.md` walks a merged change through to an installable release.

Write mode reads the facts from the repository instead of trusting the caller:
the board key comes from `BOARD_NAME` in `platformio.ini` (the string the
firmware reports in `RESP_DEVICE_INFO.board`), the version from `version.txt`
(the same file `version_script.py` embeds as `FW_VERSION`), and the chip, size
and checksum from the image itself. Anything that disagrees is refused.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import sys
from pathlib import Path

SCHEMA_VERSION = 1

# app0 in the board's partition table (tools/partitions/max_app_8MB.csv):
# offset 0x10000, size 0x7E0000. This is the whole slot the app may write.
ADDRESS = 0x10000
LAYOUT = "max_app_8MB"
LAYOUT_PARTITION_FILE = "max_app_8MB.csv"
APP_SLOT_BYTES = 0x7E0000

# ESP-IDF application image layout: 0xE9 magic, then the chip id at offset 12.
IMAGE_MAGIC = 0xE9
CHIP_ID_OFFSET = 12
CHIP_ID_ESP32_S3 = 0x0009
CHIP = "esp32s3"

ASSET_TEMPLATE = "LucydDeck-firmware-{tag}.bin"
DEFAULT_REPO = "lucyd-dev/LucydDeck-firmware"
REPO_ROOT = Path(__file__).resolve().parent.parent
WIKI = "wiki/Firmware-Manifest.md"

TOP_LEVEL_FIELDS = ("schemaVersion", "version", "releaseUrl", "boards")
BOARD_FIELDS = ("asset", "size", "sha256", "chip", "address", "layout")


class ManifestError(Exception):
    """The image or the manifest would not survive the app's checks."""


def fail(message: str) -> None:
    raise ManifestError(message)


def expect(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except OSError as error:
        fail(f"cannot read {path}: {error}")


def firmware_version(root: Path) -> str:
    text = read_text(root / "version.txt").strip()
    expect(
        re.fullmatch(r"\d+\.\d+\.\d+", text) is not None,
        f"version.txt holds {text!r}, which is not MAJOR.MINOR.PATCH",
    )
    return text


def board_name(root: Path) -> str:
    platformio = read_text(root / "platformio.ini")
    match = re.search(r'-D\s+BOARD_NAME=\\?"([A-Za-z0-9_]+)\\?"', platformio)
    expect(
        match is not None,
        'platformio.ini has no -D BOARD_NAME="..." in [common]; the manifest '
        "board key must be the string the firmware reports",
    )
    return match.group(1)


def board_partition_file(root: Path) -> str:
    platformio = read_text(root / "platformio.ini")
    match = re.search(r"^board\s*=\s*(\S+)\s*$", platformio, re.MULTILINE)
    expect(match is not None, "platformio.ini has no board = ... line")
    board = match.group(1)
    board_json = root / "boards" / f"{board}.json"
    try:
        config = json.loads(read_text(board_json))
    except json.JSONDecodeError as error:
        fail(f"cannot parse {board_json}: {error}")
    return config.get("build", {}).get("arduino", {}).get("partitions", "")


def image_digest(image: bytes) -> str:
    return hashlib.sha256(image).hexdigest()


def check_image(image: bytes, label: str) -> None:
    """Refuse an image the deck cannot hold or the ROM loader cannot boot."""
    expect(len(image) > 0, f"{label} is empty")
    expect(
        len(image) <= APP_SLOT_BYTES,
        f"{label} is {len(image)} bytes, over the {APP_SLOT_BYTES}-byte "
        f"{LAYOUT} app slot (limit 8064 KiB)",
    )
    expect(
        image[0] == IMAGE_MAGIC,
        f"{label} does not start with the 0x{IMAGE_MAGIC:02X} application image "
        "magic; point --bin at the build's firmware.bin",
    )
    expect(
        len(image) >= CHIP_ID_OFFSET + 2,
        f"{label} is too short to carry an application image header",
    )
    (chip_id,) = struct.unpack_from("<H", image, CHIP_ID_OFFSET)
    expect(
        chip_id == CHIP_ID_ESP32_S3,
        f"{label} is built for chip id {chip_id}, not ESP32-S3 ({CHIP_ID_ESP32_S3})",
    )


def build_manifest(
    image: bytes, version: str, tag: str, repo: str, board: str
) -> dict:
    asset = ASSET_TEMPLATE.format(tag=tag)
    return {
        "schemaVersion": SCHEMA_VERSION,
        "version": version,
        "releaseUrl": release_url(repo, tag),
        "boards": {
            board: {
                "asset": asset,
                "size": len(image),
                "sha256": image_digest(image),
                "chip": CHIP,
                "address": ADDRESS,
                "layout": LAYOUT,
            }
        },
    }


def release_url(repo: str, tag: str) -> str:
    return f"https://github.com/{repo}/releases/tag/{tag}"


def validate(
    manifest: object, image: bytes, version: str, tag: str, repo: str, board: str
) -> None:
    """Every check the app or the release workflow makes before publishing."""
    expect(isinstance(manifest, dict), "the manifest is not a JSON object")
    expect(
        tuple(manifest) == TOP_LEVEL_FIELDS,
        "top-level fields are "
        + (", ".join(manifest) or "none")
        + "; expected exactly "
        + ", ".join(TOP_LEVEL_FIELDS),
    )
    expect(
        type(manifest["schemaVersion"]) is int and manifest["schemaVersion"] == SCHEMA_VERSION,
        f"schemaVersion must be {SCHEMA_VERSION}, the only version the app reads",
    )
    expect(
        manifest["version"] == version,
        f"version is {manifest['version']!r} but {version!r} was expected "
        f"for tag {tag}",
    )
    expect(
        manifest["releaseUrl"] == release_url(repo, tag),
        f"releaseUrl must be {release_url(repo, tag)}",
    )

    boards = manifest["boards"]
    expect(isinstance(boards, dict), "boards must be an object keyed by board name")
    expect(
        tuple(boards) == (board,),
        "boards must key exactly "
        f"{board} (the string RESP_DEVICE_INFO.board reports); got "
        + (", ".join(boards) or "none"),
    )

    entry = boards[board]
    expect(isinstance(entry, dict), f"boards.{board} must be an object")
    expect(
        tuple(entry) == BOARD_FIELDS,
        f"boards.{board} fields are "
        + (", ".join(entry) or "none")
        + "; expected exactly "
        + ", ".join(BOARD_FIELDS),
    )
    expect(
        entry["asset"] == ASSET_TEMPLATE.format(tag=tag),
        f"boards.{board}.asset must be {ASSET_TEMPLATE.format(tag=tag)}",
    )
    expect(
        type(entry["size"]) is int and entry["size"] == len(image),
        f"boards.{board}.size is {entry['size']!r} but the image is {len(image)} bytes",
    )
    expect(
        entry["sha256"] == image_digest(image),
        f"boards.{board}.sha256 does not match the image",
    )
    expect(
        entry["chip"] == CHIP,
        f"boards.{board}.chip must be {CHIP}, the only chip the app knows",
    )
    expect(
        type(entry["address"]) is int and entry["address"] == ADDRESS,
        f"boards.{board}.address must be {ADDRESS} (0x{ADDRESS:X}); "
        "anything else would write outside the app partition",
    )
    expect(
        entry["layout"] == LAYOUT,
        f"boards.{board}.layout must be {LAYOUT}, the only layout the app knows",
    )
    expect(
        entry["size"] <= APP_SLOT_BYTES,
        f"boards.{board}.size is over the {APP_SLOT_BYTES}-byte app slot "
        "(limit 8064 KiB)",
    )


def write_manifest(path: Path, manifest: dict) -> None:
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def read_manifest(path: Path) -> object:
    try:
        return json.loads(read_text(path))
    except json.JSONDecodeError as error:
        fail(f"cannot parse {path}: {error}")


def summarise(manifest: dict, board: str, action: str) -> None:
    entry = manifest["boards"][board]
    print(f"{action} {manifest['version']}")
    print(f"  board    {board}")
    print(f"  asset    {entry['asset']}")
    print(f"  size     {entry['size']} bytes ({APP_SLOT_BYTES} available)")
    print(f"  sha256   {entry['sha256']}")
    print(f"  chip     {entry['chip']} at {entry['address']}, layout {entry['layout']}")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Write or check the board-keyed firmware manifest.",
        epilog=f"Reference: {WIKI}",
    )
    parser.add_argument(
        "--bin",
        required=True,
        type=Path,
        help="the built app image (firmware.bin)",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("firmware.json"),
        help="where to write the manifest (default: firmware.json)",
    )
    parser.add_argument(
        "--check",
        type=Path,
        metavar="MANIFEST",
        help="validate an existing manifest against the image instead of writing one",
    )
    parser.add_argument(
        "--version",
        help="override the version read from version.txt",
    )
    parser.add_argument(
        "--tag",
        help="release tag (default: v<version>); must match the version",
    )
    parser.add_argument(
        "--repo",
        default=os.environ.get("GITHUB_REPOSITORY", DEFAULT_REPO),
        help=f"owner/name used in releaseUrl (default: {DEFAULT_REPO})",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=REPO_ROOT,
        help="repository root (default: the parent of this script)",
    )
    return parser.parse_args(argv)


def released_version(manifest: object) -> str:
    """The version a checked manifest names, before anything else is trusted."""
    version = manifest.get("version") if isinstance(manifest, dict) else None
    expect(
        isinstance(version, str)
        and re.fullmatch(r"\d+\.\d+\.\d+", version) is not None,
        f"the manifest names version {version!r}, which is not MAJOR.MINOR.PATCH",
    )
    return version


def release_tag(version: str, tag: str | None) -> str:
    """The tag a version releases under, refusing a tag that names another."""
    resolved = tag or f"v{version}"
    expect(
        resolved == f"v{version}",
        f"tag {resolved} does not carry version {version}; the manifest and the "
        "release must name the same version",
    )
    return resolved


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        expect(
            board_partition_file(args.root) == LAYOUT_PARTITION_FILE,
            f"the board no longer builds with {LAYOUT_PARTITION_FILE}; "
            f"layout {LAYOUT} would be a lie",
        )
        board = board_name(args.root)

        try:
            image = args.bin.read_bytes()
        except OSError as error:
            fail(f"cannot read {args.bin}: {error}")
        check_image(image, str(args.bin))

        if args.check is not None:
            # A published manifest is checked against the release it belongs to,
            # not against the checkout that happens to run the check.
            manifest = read_manifest(args.check)
            version = args.version or released_version(manifest)
            tag = release_tag(version, args.tag)
            validate(manifest, image, version, tag, args.repo, board)
            summarise(manifest, board, f"{args.check} is valid for")
            return 0

        version = args.version or firmware_version(args.root)
        expect(
            re.fullmatch(r"\d+\.\d+\.\d+", version) is not None,
            f"version {version!r} is not MAJOR.MINOR.PATCH",
        )
        tag = release_tag(version, args.tag)
        manifest = build_manifest(image, version, tag, args.repo, board)
        validate(manifest, image, version, tag, args.repo, board)
        write_manifest(args.out, manifest)
        summarise(manifest, board, f"wrote {args.out} for")
        return 0
    except ManifestError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
