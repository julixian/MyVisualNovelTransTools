#!/usr/bin/env python3
import argparse
import os
import shutil

from bruteforce_decoder import load_loose_json


def ensure_unique_path(path):
    if not os.path.exists(path):
        return path

    base, ext = os.path.splitext(path)
    index = 2
    while True:
        candidate = f"{base}__dup{index}{ext}"
        if not os.path.exists(candidate):
            return candidate
        index += 1


def choose_restored_name(source_name, mapping):
    if source_name in mapping and mapping[source_name]:
        return mapping[source_name][0], 'full'

    stem, ext = os.path.splitext(source_name)
    if ext and stem in mapping and mapping[stem]:
        return mapping[stem][0] + ext, 'stem'

    return None, None


def restore_filenames(source_dir, output_dir, mapping, copy_unmapped=False):
    os.makedirs(output_dir, exist_ok=True)

    copied_count = 0
    skipped_count = 0
    duplicate_count = 0

    for entry in sorted(os.listdir(source_dir)):
        source_path = os.path.join(source_dir, entry)
        if not os.path.isfile(source_path):
            continue

        restored_name, matched_by = choose_restored_name(entry, mapping)
        if restored_name is None:
            if not copy_unmapped:
                skipped_count += 1
                print(f"[skip] {entry} -> no mapped filename")
                continue

            restored_name = entry
            matched_by = 'original'

        destination_path = os.path.join(output_dir, restored_name)
        final_path = ensure_unique_path(destination_path)
        if final_path != destination_path:
            duplicate_count += 1

        shutil.copy2(source_path, final_path)
        copied_count += 1
        print(f"[copy] {entry} -> {os.path.basename(final_path)} ({matched_by})")

    print('-' * 40)
    print(f"Copied files: {copied_count}")
    print(f"Skipped files: {skipped_count}")
    print(f"Resolved duplicates: {duplicate_count}")
    print(f"Output directory: {output_dir}")


def main():
    parser = argparse.ArgumentParser(
        description='Copy files to a new folder using the first candidate in cracked_filenames.json.'
    )
    parser.add_argument('source_dir', help='Directory containing the original obfuscated files')
    parser.add_argument('output_dir', help='Directory to write restored copies into')
    parser.add_argument(
        '--json',
        dest='json_path',
        default='cracked_filenames.json',
        help='Path to cracked_filenames.json',
    )
    parser.add_argument(
        '--copy-unmapped',
        action='store_true',
        help='Also copy files with no JSON candidate using their original filename',
    )

    args = parser.parse_args()

    if not os.path.isdir(args.source_dir):
        parser.error(f"Source directory does not exist: {args.source_dir}")

    mapping = load_loose_json(args.json_path)
    restore_filenames(args.source_dir, args.output_dir, mapping, copy_unmapped=args.copy_unmapped)


if __name__ == '__main__':
    main()
