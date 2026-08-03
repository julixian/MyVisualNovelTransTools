#!/usr/bin/env python3
"""Encrypt RLD payloads used by the game."""

from __future__ import annotations

import argparse
from pathlib import Path

from rld_codec import add_key_arguments, load_key_sets, transform_directory


BASE_DIR = Path(__file__).resolve().parent


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Encrypt .bin files to .rld files.",
    )
    parser.add_argument(
        "input_dir",
        nargs="?",
        type=Path,
        default=BASE_DIR / "rld_injected",
        help="directory containing .bin files (default: %(default)s)",
    )
    parser.add_argument(
        "output_dir",
        nargs="?",
        type=Path,
        default=BASE_DIR / "rld_enc",
        help="directory for encrypted .rld files (default: %(default)s)",
    )
    add_key_arguments(parser, BASE_DIR)
    parser.add_argument("--quiet", action="store_true", help="only print the summary")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        key_sets = load_key_sets(args)
        results = transform_directory(
            args.input_dir,
            args.output_dir,
            input_suffix=".bin",
            output_suffix=".rld",
            key_sets=key_sets,
        )
    except (OSError, ValueError) as exc:
        raise SystemExit(f"error: {exc}") from exc

    if not args.quiet:
        for result in results:
            print(
                f"{result.source.name} -> {result.destination.name}: "
                f"{result.file_size} bytes, transformed {result.transformed_bytes} bytes"
            )
    print(f"encrypted {len(results)} file(s) into {args.output_dir.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
