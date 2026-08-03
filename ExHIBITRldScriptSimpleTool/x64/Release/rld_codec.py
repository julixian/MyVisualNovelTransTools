"""Shared RLD codec and key loading helpers."""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path


HEADER_SIZE = 0x10
MAX_TRANSFORMED_BYTES = 0xFFC0
KEY_WORD_COUNT = 0x100
WORD_SIZE = 4
UINT32_MAX = 0xFFFFFFFF


@dataclass(frozen=True, slots=True)
class KeySet:
    table: tuple[int, ...]
    xor_key: int


@dataclass(frozen=True, slots=True)
class TransformResult:
    source: Path
    destination: Path
    file_size: int
    transformed_bytes: int


def read_key_table(path: Path) -> tuple[int, ...]:
    data = path.read_bytes()
    expected_size = KEY_WORD_COUNT * WORD_SIZE
    if len(data) != expected_size:
        raise ValueError(
            f"{path} must contain exactly {expected_size} bytes "
            f"({KEY_WORD_COUNT} little-endian uint32 values); got {len(data)}"
        )
    return struct.unpack(f"<{KEY_WORD_COUNT}I", data)


def read_xor_key(path: Path) -> int:
    value_text: str | None = None
    for raw_line in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw_line.split("#", 1)[0].split(";", 1)[0].strip()
        if not line:
            continue
        if "=" in line:
            line = line.rsplit("=", 1)[1].strip()
        value_text = line.split()[0].rstrip(",")
        break

    if value_text is None:
        raise ValueError(f"{path} does not contain a key")

    try:
        if value_text.lower().startswith(("0x", "0o", "0b")):
            value = int(value_text, 0)
        elif any(character in "abcdefABCDEF" for character in value_text):
            value = int(value_text, 16)
        else:
            value = int(value_text, 10)
    except ValueError as exc:
        raise ValueError(f"invalid uint32 key in {path}: {value_text!r}") from exc

    if not 0 <= value <= UINT32_MAX:
        raise ValueError(f"key in {path} is outside the uint32 range: {value}")
    return value


def transform_rld(data: bytes, key_set: KeySet) -> tuple[bytes, int]:
    if len(data) < HEADER_SIZE:
        raise ValueError(
            f"RLD data is shorter than the {HEADER_SIZE}-byte header: {len(data)} bytes"
        )
    if len(key_set.table) != KEY_WORD_COUNT:
        raise ValueError(
            f"key table must contain {KEY_WORD_COUNT} values; got {len(key_set.table)}"
        )

    # The game transforms at most 0x3ff0 complete DWORDs after the header.
    # Do not truncate the payload word count to 16 bits before applying this cap.
    word_count = min(
        (len(data) - HEADER_SIZE) // WORD_SIZE,
        MAX_TRANSFORMED_BYTES // WORD_SIZE,
    )
    output = bytearray(data)
    for index in range(word_count):
        offset = HEADER_SIZE + index * WORD_SIZE
        value = struct.unpack_from("<I", output, offset)[0]
        value ^= key_set.table[index & 0xFF] ^ key_set.xor_key
        struct.pack_into("<I", output, offset, value)
    return bytes(output), word_count * WORD_SIZE


def transform_directory(
    input_dir: Path,
    output_dir: Path,
    *,
    input_suffix: str,
    output_suffix: str,
    key_sets: tuple[KeySet, KeySet],
) -> list[TransformResult]:
    if not input_dir.is_dir():
        raise ValueError(f"input directory does not exist: {input_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    normal_key_set, def_key_set = key_sets
    sources = sorted(
        (
            path
            for path in input_dir.iterdir()
            if path.is_file() and path.suffix.casefold() == input_suffix.casefold()
        ),
        key=lambda path: path.name.casefold(),
    )

    results: list[TransformResult] = []
    for source in sources:
        key_set = def_key_set if source.stem.casefold() == "def" else normal_key_set
        data = source.read_bytes()
        transformed, transformed_bytes = transform_rld(data, key_set)
        destination = output_dir / f"{source.stem}{output_suffix}"
        destination.write_bytes(transformed)
        results.append(
            TransformResult(
                source=source,
                destination=destination,
                file_size=len(data),
                transformed_bytes=transformed_bytes,
            )
        )
    return results


def add_key_arguments(parser: argparse.ArgumentParser, base_dir: Path) -> None:
    parser.add_argument(
        "--key-table",
        type=Path,
        default=base_dir / "key.bin",
        help="normal 256-word key table (default: %(default)s)",
    )
    parser.add_argument(
        "--key",
        type=Path,
        default=base_dir / "key.txt",
        help="text file containing the normal XOR key (default: %(default)s)",
    )
    parser.add_argument(
        "--def-key-table",
        type=Path,
        default=base_dir / "key_def.bin",
        help="def.rld 256-word key table (default: %(default)s)",
    )
    parser.add_argument(
        "--def-key",
        type=Path,
        default=base_dir / "key_def.txt",
        help="text file containing the def.rld XOR key (default: %(default)s)",
    )


def load_key_sets(args: argparse.Namespace) -> tuple[KeySet, KeySet]:
    return (
        KeySet(read_key_table(args.key_table), read_xor_key(args.key)),
        KeySet(read_key_table(args.def_key_table), read_xor_key(args.def_key)),
    )
