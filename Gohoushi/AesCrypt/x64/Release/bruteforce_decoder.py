#!/usr/bin/env python3
import argparse
import itertools
import json
import math
import os
import re
import string


VALID_CHARS = string.ascii_lowercase + string.digits + '_'

FAMILY_PATTERNS = [
    ("gh_story", re.compile(r"^gh_[a-z]\d{2}[a-z]?$"), 120.0),
    ("event", re.compile(r"^ev[a-z]{2,4}\d{1,3}$"), 110.0),
    ("scene", re.compile(r"^(?:ru|to|sh|ma|ib|td|wn)_[a-z]{2,5}$"), 100.0),
    ("special", re.compile(r"^(?:bg|bgm|se|init|icons)$"), 95.0),
]


def transform_char(c, i, offset):
    is_odd_position = (i & 1) != 0

    if '0' <= c <= '9':
        if is_odd_position:
            j = ord(c) + ord('a') - ord('0')
        else:
            j = ord('z') - (ord(c) - ord('0'))
    elif 'a' <= c <= 'z':
        if is_odd_position:
            j = ord('z') - ord(c) + ord('a')
        else:
            j = ord(c)
    elif c == '_':
        j = ord('a')
    else:
        return None

    j += offset
    while j > ord('z'):
        j -= 25

    return chr(j)


def dedupe_preserve_order(items):
    seen = set()
    unique_items = []

    for item in items:
        if item in seen:
            continue
        seen.add(item)
        unique_items.append(item)

    return unique_items


def crack_obfuscated_name(obf_name):
    if len(obf_name) >= 13:
        return []

    valid_originals = []
    length = len(obf_name)

    for assumed_offset in range(26):
        pos_candidates = []
        possible = True

        for index in range(length):
            target_char = obf_name[index]
            valid_for_pos = []

            for candidate_char in VALID_CHARS:
                if transform_char(candidate_char, index, assumed_offset) == target_char:
                    valid_for_pos.append(candidate_char)

            if not valid_for_pos:
                possible = False
                break

            pos_candidates.append(valid_for_pos)

        if not possible:
            continue

        for combo in itertools.product(*pos_candidates):
            candidate_str = "".join(combo)
            actual_offset = sum(ord(c) for c in candidate_str) % 26
            if actual_offset == assumed_offset:
                valid_originals.append(candidate_str)

    return dedupe_preserve_order(valid_originals)


def load_loose_json(path):
    with open(path, 'r', encoding='utf-8') as handle:
        raw_text = handle.read()

    sanitized = re.sub(r',(?=\s*[\]}])', '', raw_text)
    data = json.loads(sanitized)

    normalized = {}
    for key, value in data.items():
        if isinstance(value, list):
            normalized[key] = dedupe_preserve_order(value)
        elif value is None:
            normalized[key] = []
        else:
            normalized[key] = [value]

    return normalized


def load_mapping_names(path):
    if not path or not os.path.exists(path):
        return set()

    names = set()
    with open(path, 'r', encoding='utf-8-sig', errors='ignore') as handle:
        for index, line in enumerate(handle):
            line = line.strip()
            if not line:
                continue
            if index == 0 and ',' in line:
                continue

            parts = [part.strip() for part in line.split(',', 1)]
            if not parts:
                continue

            name = parts[0]
            if re.fullmatch(r'[a-z0-9_]+', name):
                names.add(name)

    return names


def iter_ngrams(text, n):
    padded = f"^{text}$"
    for index in range(len(padded) - n + 1):
        yield padded[index:index + n]


def build_ngram_stats(seed_names):
    bigrams = {}
    trigrams = {}

    for name in seed_names:
        for gram in iter_ngrams(name, 2):
            bigrams[gram] = bigrams.get(gram, 0) + 1
        for gram in iter_ngrams(name, 3):
            trigrams[gram] = trigrams.get(gram, 0) + 1

    return bigrams, trigrams


def score_candidate(candidate, known_names, ngram_stats):
    score = 0.0
    matched_family = None

    for family_name, pattern, bonus in FAMILY_PATTERNS:
        if pattern.fullmatch(candidate):
            matched_family = family_name
            score += bonus
            break

    if candidate in known_names:
        score += 18.0

    if candidate.isalpha():
        score += 6.0
    if re.fullmatch(r'[a-z]{2}_[a-z]{2,5}', candidate):
        score += 6.0
    if re.fullmatch(r'[a-z]{3,6}\d{1,3}', candidate):
        score += 6.0

    if candidate.endswith('_'):
        score -= 12.0
    if candidate.startswith('_'):
        score -= 8.0
    if '__' in candidate:
        score -= 10.0
    if re.search(r'\d[a-z]_', candidate):
        score -= 4.0

    bigrams, trigrams = ngram_stats
    for gram in iter_ngrams(candidate, 2):
        score += math.log1p(bigrams.get(gram, 0)) * 1.8
    for gram in iter_ngrams(candidate, 3):
        score += math.log1p(trigrams.get(gram, 0)) * 2.5

    return score, matched_family is not None


def reduce_candidates(results, mapping_csv=None):
    seed_names = load_mapping_names(mapping_csv)
    for candidates in results.values():
        if len(candidates) == 1:
            seed_names.add(candidates[0])

    ngram_stats = build_ngram_stats(seed_names)
    reduced = {}
    collapsed_count = 0
    unresolved_count = 0

    for obfuscated_name, candidates in results.items():
        unique_candidates = dedupe_preserve_order(candidates)
        if len(unique_candidates) <= 1:
            reduced[obfuscated_name] = unique_candidates
            continue

        ranked = []
        for candidate in unique_candidates:
            score, strong_match = score_candidate(candidate, seed_names, ngram_stats)
            ranked.append((candidate, score, strong_match))

        ranked.sort(key=lambda item: (-item[1], item[0]))
        best_candidate, best_score, best_is_strong = ranked[0]
        second_score = ranked[1][1]
        score_gap = best_score - second_score

        if (best_is_strong and score_gap >= 8.0) or (best_score >= 60.0 and score_gap >= 14.0):
            reduced[obfuscated_name] = [best_candidate]
            collapsed_count += 1
        else:
            reduced[obfuscated_name] = [candidate for candidate, _, _ in ranked]
            unresolved_count += 1

    return reduced, collapsed_count, unresolved_count


def crack_source_dir(source_dir):
    if not os.path.exists(source_dir):
        raise FileNotFoundError(f"Source directory does not exist: {source_dir}")

    source_files = os.listdir(source_dir)
    results = {}

    print(f"Scanning {len(source_files)} entries from '{source_dir}'...")

    processed_count = 0
    cracked_count = 0
    for filename in source_files:
        full_path = os.path.join(source_dir, filename)
        if os.path.isdir(full_path):
            continue

        processed_count += 1
        possible_names = crack_obfuscated_name(filename)

        if possible_names:
            cracked_count += 1
            print(f"[ok] {filename} -> {len(possible_names)} candidate(s)")
        else:
            print(f"[skip] {filename} -> no candidate")

        results[filename] = possible_names

    print('-' * 40)
    print(f"Processed files: {processed_count}")
    print(f"Recovered names: {cracked_count}")

    return results


def main():
    parser = argparse.ArgumentParser(
        description='Bruteforce obfuscated bundle names, or post-process an existing cracked_filenames.json.'
    )
    parser.add_argument('source_dir', nargs='?', help='Directory that contains obfuscated filenames')
    parser.add_argument('--from-json', dest='input_json', help='Existing cracked_filenames.json to post-process')
    parser.add_argument('--output', default='cracked_filenames.json', help='Output JSON path')
    parser.add_argument(
        '--choose-best',
        action='store_true',
        help='Collapse multi-candidate entries when a strong filename pattern wins; otherwise sort candidates by plausibility',
    )
    parser.add_argument(
        '--mapping-csv',
        default='filename_mapping.csv',
        help='Optional CSV of known-good names used as scoring seeds',
    )

    args = parser.parse_args()

    if bool(args.source_dir) == bool(args.input_json):
        parser.error('Provide exactly one of source_dir or --from-json.')

    if args.input_json:
        results = load_loose_json(args.input_json)
        print(f"Loaded {len(results)} entries from '{args.input_json}'.")
    else:
        results = crack_source_dir(args.source_dir)

    if args.choose_best:
        results, collapsed_count, unresolved_count = reduce_candidates(results, args.mapping_csv)
        print('-' * 40)
        print(f"Auto-collapsed entries: {collapsed_count}")
        print(f"Still ambiguous after ranking: {unresolved_count}")

    with open(args.output, 'w', encoding='utf-8') as handle:
        json.dump(results, handle, indent=4, ensure_ascii=False)

    print(f"Saved results to: {args.output}")


if __name__ == '__main__':
    main()
