#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any


TEXT_HEADER_SIZE = 16
SCRIPT_HEADER_SIZE = 12
SCRIPT_MAGIC = 0x30327653
TEXT_XOR_KEY = 0xF7D5859D
IMMEDIATE_OPCODE = 0x1001F
LOCAL_REF_BASE = 0x40000000
FRAME_REF_BASE = 0x80000000
REF_TYPE_MASK = 0xF0000000

# Top-level opcodes that consume one immediate dword after the opcode.
SCRIPT_IMMEDIATE_OPS = {
    0x10001,
    0x10009,
    0x1000A,
    0x1000B,
    0x10014,
    0x10015,
    0x10016,
    0x10018,
    0x10019,
    0x1001D,
    0x1001E,
    0x1001F,
    0x10020,
    0x10021,
}

# For the current game, `text` and `text_w` cover the actual dialogue flow.
# The rest are left here so the scanner can keep supporting them when they do
# appear in other Softpal titles.
TEXT_OPCODE_SPECS: dict[int, tuple[str, list[str]]] = {
    0x20002: ("text", ["wait_value", "message_offset", "name_offset", "voice_ref"]),
    0x2000F: ("text_w", ["wait_value", "message_offset", "name_offset", "voice_ref"]),
    0x20010: ("text_a", ["message_offset", "name_offset", "voice_ref"]),
    0x20011: ("text_wa", ["message_offset", "name_offset", "voice_ref"]),
    0x20012: ("text_n", ["wait_value", "message_offset", "name_offset", "voice_ref"]),
    0x20013: ("text_cat", ["wait_value", "message_offset", "name_offset", "extra_ref"]),
    0x20014: ("text_history", ["message_offset", "name_offset", "voice_ref"]),
    0x20034: ("text_history_alt", ["message_offset", "name_offset", "voice_ref"]),
    0x2003E: ("text_h", ["wait_value", "message_offset", "name_offset", "voice_ref"]),
    0x2003F: ("text_wh", ["wait_value", "message_offset", "name_offset", "voice_ref"]),
}


@dataclass(frozen=True)
class TextRecord:
    index: int
    offset: int
    meta: int
    raw_bytes: bytes
    text: str
    decode_ok: bool


@dataclass(frozen=True)
class DialogueEntry:
    index: int
    script_offset: int
    opcode: int
    opcode_name: str
    wait_value: int | None
    message_offset: int | None
    name_offset: int | None
    voice_ref: int | None
    extra_ref: int | None
    wait_push_offset: int | None
    message_push_offset: int | None
    name_push_offset: int | None
    voice_push_offset: int | None
    extra_push_offset: int | None
    message: str
    name: str
    message_meta: int | None
    name_meta: int | None


@dataclass(frozen=True)
class PushResolution:
    push_offset: int
    raw_operand: int
    resolved_value: int | None
    source_kind: str
    source_ref: int | None
    assign_offset: int | None


class SoftpalToolError(RuntimeError):
    pass


def u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def p32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def rol8(value: int, shift: int) -> int:
    shift &= 7
    if shift == 0:
        return value & 0xFF
    return ((value << shift) | (value >> (8 - shift))) & 0xFF


def ror8(value: int, shift: int) -> int:
    shift &= 7
    if shift == 0:
        return value & 0xFF
    return ((value >> shift) | ((value << (8 - shift)) & 0xFF)) & 0xFF


def softpal_transform_decode(data: bytes) -> bytes:
    buf = bytearray(data)
    size = len(buf) & ~3
    rotate = 4
    for offset in range(0, size, 4):
        buf[offset] = rol8(buf[offset], rotate)
        p32(buf, offset, u32(buf, offset) ^ TEXT_XOR_KEY)
        rotate += 1
    return bytes(buf)


def softpal_transform_encode(data: bytes) -> bytes:
    buf = bytearray(data)
    size = len(buf) & ~3
    rotate = 4
    for offset in range(0, size, 4):
        p32(buf, offset, u32(buf, offset) ^ TEXT_XOR_KEY)
        buf[offset] = ror8(buf[offset], rotate)
        rotate += 1
    return bytes(buf)


def decode_cp932(raw: bytes) -> tuple[str, bool]:
    try:
        return raw.decode("cp932"), True
    except UnicodeDecodeError:
        return raw.decode("cp932", errors="replace"), False


def encode_cp932(text: str) -> bytes:
    try:
        return text.encode("cp932")
    except UnicodeEncodeError as exc:
        raise SoftpalToolError(
            f"文本无法编码为 CP932: {text!r} ({exc})"
        ) from exc


def load_text_records(path: Path, decode_body: bool = False) -> tuple[bytes, list[TextRecord]]:
    raw = path.read_bytes()
    if len(raw) < TEXT_HEADER_SIZE:
        raise SoftpalToolError(f"{path} 太小，不像有效的 TEXT.DAT")

    normalized = raw
    if decode_body:
        normalized = raw[:TEXT_HEADER_SIZE] + softpal_transform_decode(raw[TEXT_HEADER_SIZE:])

    count = u32(normalized, 0x0C)
    records: list[TextRecord] = []
    offset = TEXT_HEADER_SIZE
    index = 0
    while offset < len(normalized):
        if offset + 4 > len(normalized):
            raise SoftpalToolError(f"{path} 在 0x{offset:X} 处记录被截断")
        meta = u32(normalized, offset)
        end = normalized.find(b"\x00", offset + 4)
        if end == -1:
            raise SoftpalToolError(f"{path} 在 0x{offset:X} 处缺少字符串终止符")
        raw_bytes = normalized[offset + 4 : end]
        text, decode_ok = decode_cp932(raw_bytes)
        records.append(
            TextRecord(
                index=index,
                offset=offset,
                meta=meta,
                raw_bytes=raw_bytes,
                text=text,
                decode_ok=decode_ok,
            )
        )
        index += 1
        offset = end + 1

    if count != len(records):
        raise SoftpalToolError(
            f"{path} 头部条目数是 {count}，实际解析出 {len(records)} 条"
        )
    return normalized, records


def load_script(path: Path) -> bytes:
    raw = path.read_bytes()
    if len(raw) < SCRIPT_HEADER_SIZE:
        raise SoftpalToolError(f"{path} 太小，不像有效的 SCRIPT.SRC")
    if u32(raw, 0) != SCRIPT_MAGIC:
        raise SoftpalToolError(f"{path} magic 不匹配，期待 Sv20")
    return raw


def resolve_text(records_by_offset: dict[int, TextRecord], offset: int | None) -> tuple[str, int | None]:
    if offset is None or offset in (-1, 0x0FFFFFFF, 0xFFFFFFFF):
        return "", None
    record = records_by_offset.get(offset)
    if not record:
        return "", None
    return record.text, record.meta


def is_local_ref(value: int) -> bool:
    return (value & REF_TYPE_MASK) == LOCAL_REF_BASE


def is_frame_ref(value: int) -> bool:
    return (value & REF_TYPE_MASK) == FRAME_REF_BASE


def frame_slot_index(value: int | None) -> int | None:
    if value is None or not is_frame_ref(value):
        return None
    return value & 0xFFFF


def find_latest_local_assignment(script: bytes, before_offset: int, local_ref: int) -> tuple[int, int] | None:
    cursor = before_offset - 12
    while cursor >= SCRIPT_HEADER_SIZE:
        if cursor + 12 <= len(script) and u32(script, cursor) == 0x10001:
            dst = u32(script, cursor + 4)
            src = u32(script, cursor + 8)
            if dst == local_ref:
                return cursor, src
        cursor -= 4
    return None


def resolve_push_operand(script: bytes, push_offset: int, operand: int) -> PushResolution:
    current = operand
    current_before = push_offset
    visited: set[int] = set()

    while is_local_ref(current):
        if current in visited:
            return PushResolution(
                push_offset=push_offset,
                raw_operand=operand,
                resolved_value=None,
                source_kind="local_cycle",
                source_ref=current,
                assign_offset=None,
            )
        visited.add(current)
        assigned = find_latest_local_assignment(script, current_before, current)
        if assigned is None:
            return PushResolution(
                push_offset=push_offset,
                raw_operand=operand,
                resolved_value=None,
                source_kind="local_ref",
                source_ref=current,
                assign_offset=None,
            )
        assign_offset, current = assigned
        current_before = assign_offset

    if is_frame_ref(current):
        return PushResolution(
            push_offset=push_offset,
            raw_operand=operand,
            resolved_value=None,
            source_kind="frame_ref",
            source_ref=current,
            assign_offset=current_before if current_before != push_offset else None,
        )

    return PushResolution(
        push_offset=push_offset,
        raw_operand=operand,
        resolved_value=current,
        source_kind="immediate",
        source_ref=current,
        assign_offset=current_before if current_before != push_offset else None,
    )


def parse_text_pushes(
    script: bytes,
    script_offset: int,
    arg_names: list[str],
) -> tuple[dict[str, PushResolution], dict[str, int | None], dict[str, int | None]] | None:
    pushes: list[PushResolution] = []
    cursor = script_offset - 4
    while cursor >= SCRIPT_HEADER_SIZE and len(pushes) < len(arg_names):
        if cursor + 8 <= len(script) and u32(script, cursor) == IMMEDIATE_OPCODE:
            operand = u32(script, cursor + 4)
            pushes.append(resolve_push_operand(script, cursor, operand))
            cursor -= 4
            continue
        cursor -= 4

    if len(pushes) != len(arg_names):
        return None
    pushes.reverse()

    resolutions: dict[str, PushResolution] = {}
    values: dict[str, int | None] = {}
    push_offsets: dict[str, int | None] = {}
    for name, resolution in zip(arg_names, pushes):
        resolutions[name] = resolution
        values[name] = resolution.resolved_value
        push_offsets[name] = resolution.push_offset
    return resolutions, values, push_offsets


def extract_dialogue_entries(
    script: bytes,
    records_by_offset: dict[int, TextRecord],
) -> tuple[list[DialogueEntry], list[dict[str, Any]], list[dict[str, Any]]]:
    entries: list[DialogueEntry] = []
    forwarded: list[dict[str, Any]] = []
    unresolved: list[dict[str, Any]] = []
    index = 0
    size = len(script)

    offset = SCRIPT_HEADER_SIZE
    while offset + 12 <= size:
        opcode = u32(script, offset)

        if opcode == 0x10017:
            sub_opcode = u32(script, offset + 4)
            extra = u32(script, offset + 8)
            spec = TEXT_OPCODE_SPECS.get(sub_opcode)
            if spec:
                opcode_name, arg_names = spec
                parsed = parse_text_pushes(script, offset, arg_names)
                if parsed is None:
                    unresolved.append(
                        {
                            "script_offset": offset,
                            "opcode": sub_opcode,
                            "opcode_name": opcode_name,
                            "reason": "前面不是紧邻的 0x1001F immediate push 序列",
                            "trailer": extra,
                        }
                    )
                else:
                    resolutions, values, push_offsets = parsed
                    has_frame_forward = any(
                        resolution.source_kind == "frame_ref"
                        for resolution in resolutions.values()
                    )
                    if has_frame_forward:
                        forwarded.append(
                            {
                                "script_offset": offset,
                                "opcode": sub_opcode,
                                "opcode_name": opcode_name,
                                "trailer": extra,
                                "arguments": {
                                    name: {
                                        "raw_operand": resolution.raw_operand,
                                        "source_kind": resolution.source_kind,
                                        "source_ref": resolution.source_ref,
                                        "frame_slot": frame_slot_index(resolution.source_ref),
                                        "assign_offset": resolution.assign_offset,
                                        "push_offset": resolution.push_offset,
                                    }
                                    for name, resolution in resolutions.items()
                                },
                                "notes": [
                                    "该文本调用的参数通过当前 VM 帧槽转发，调用点本身没有直接内嵌 TEXT.DAT 偏移。",
                                    "当前版本已将其从未识别列表中剔除，并单独输出为 forwarded_text_calls.json。",
                                ],
                            }
                        )
                    else:
                        message, message_meta = resolve_text(records_by_offset, values.get("message_offset"))
                        name, name_meta = resolve_text(records_by_offset, values.get("name_offset"))
                        entries.append(
                            DialogueEntry(
                                index=index,
                                script_offset=offset,
                                opcode=sub_opcode,
                                opcode_name=opcode_name,
                                wait_value=values.get("wait_value"),
                                message_offset=values.get("message_offset"),
                                name_offset=values.get("name_offset"),
                                voice_ref=values.get("voice_ref"),
                                extra_ref=values.get("extra_ref"),
                                wait_push_offset=push_offsets.get("wait_value"),
                                message_push_offset=push_offsets.get("message_offset"),
                                name_push_offset=push_offsets.get("name_offset"),
                                voice_push_offset=push_offsets.get("voice_ref"),
                                extra_push_offset=push_offsets.get("extra_ref"),
                                message=message,
                                name=name,
                                message_meta=message_meta,
                                name_meta=name_meta,
                            )
                        )
                        index += 1
        offset += 4

    return entries, forwarded, unresolved


def collect_record_usage(dialogue_entries: list[DialogueEntry]) -> dict[int, Counter[str]]:
    usage: dict[int, Counter[str]] = {}
    for entry in dialogue_entries:
        if entry.name_offset not in (None, -1, 0x0FFFFFFF, 0xFFFFFFFF):
            usage.setdefault(entry.name_offset, Counter())["name"] += 1
        if entry.message_offset not in (None, -1, 0x0FFFFFFF, 0xFFFFFFFF):
            usage.setdefault(entry.message_offset, Counter())["message"] += 1
    return usage


def build_dialogue_json(entries: list[DialogueEntry]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for entry in entries:
        rows.append(
            {
                "name": entry.name,
                "message": entry.message,
                "index": entry.index,
                "script_offset": entry.script_offset,
                "opcode": entry.opcode,
                "opcode_name": entry.opcode_name,
                "wait_value": entry.wait_value,
                "message_offset": entry.message_offset,
                "message_meta": entry.message_meta,
                "name_offset": entry.name_offset,
                "name_meta": entry.name_meta,
                "voice_ref": entry.voice_ref,
                "extra_ref": entry.extra_ref,
                "message_push_offset": entry.message_push_offset,
                "name_push_offset": entry.name_push_offset,
            }
        )
    return rows


def build_text_records_json(records: list[TextRecord], usage: dict[int, Counter[str]]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for record in records:
        counter = usage.get(record.offset, Counter())
        if counter["message"] and counter["name"]:
            kind_guess = "mixed"
        elif counter["message"]:
            kind_guess = "message"
        elif counter["name"]:
            kind_guess = "name"
        else:
            kind_guess = "other"
        rows.append(
            {
                "offset": record.offset,
                "index": record.index,
                "meta": record.meta,
                "text": record.text,
                "decode_ok": record.decode_ok,
                "kind_guess": kind_guess,
                "used_as_name": counter["name"],
                "used_as_message": counter["message"],
            }
        )
    return rows


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def extract_command(args: argparse.Namespace) -> int:
    text_path = Path(args.text)
    script_path = Path(args.script)
    out_dir = Path(args.out)

    _, records = load_text_records(text_path, decode_body=args.decode_text_body)
    records_by_offset = {record.offset: record for record in records}
    script = load_script(script_path)
    dialogue_entries, forwarded, unresolved = extract_dialogue_entries(script, records_by_offset)
    usage = collect_record_usage(dialogue_entries)

    dialogue_json = build_dialogue_json(dialogue_entries)
    text_records_json = build_text_records_json(records, usage)
    summary = {
        "text_record_count": len(records),
        "dialogue_entry_count": len(dialogue_entries),
        "forwarded_text_call_count": len(forwarded),
        "unresolved_text_call_count": len(unresolved),
        "text_opcode_counts": Counter(entry.opcode_name for entry in dialogue_entries),
        "decode_text_body": args.decode_text_body,
        "notes": [
            "TEXT.DAT 的 name/message 引用值是文件内偏移，不是顺序 id。",
            "当前样本主对话基本落在 text(0x20002) 和 text_w(0x2000F)。",
            "text_history_alt(0x20034) 的 2 处 helper 转发调用已单独记录到 forwarded_text_calls.json。",
            "默认回注只补已识别主对话里的 name/message 偏移，安全性更高。",
        ],
    }

    write_json(out_dir / "dialogue_name_msg.json", dialogue_json)
    write_json(out_dir / "text_records.json", text_records_json)
    write_json(out_dir / "summary.json", summary)
    if forwarded:
        write_json(out_dir / "forwarded_text_calls.json", forwarded)
    else:
        forwarded_path = out_dir / "forwarded_text_calls.json"
        if forwarded_path.exists():
            forwarded_path.unlink()
    if unresolved:
        write_json(out_dir / "unresolved_text_calls.json", unresolved)
    else:
        unresolved_path = out_dir / "unresolved_text_calls.json"
        if unresolved_path.exists():
            unresolved_path.unlink()

    print(f"[extract] TEXT 记录: {len(records)}")
    print(f"[extract] 对话条目: {len(dialogue_entries)}")
    print(f"[extract] 已识别的转发型文本调用: {len(forwarded)}")
    print(f"[extract] 未直接识别的文本调用: {len(unresolved)}")
    print(f"[extract] 输出目录: {out_dir}")
    return 0


def translated_field(item: dict[str, Any], base_key: str, original: str) -> str | None:
    translated_key = f"{base_key}_translated"
    translated = item.get(translated_key)
    if isinstance(translated, str) and translated != "":
        return translated

    current = item.get(base_key)
    if isinstance(current, str) and current != original:
        return current
    return None


def add_translation(
    translations: dict[int, str],
    offset: int,
    value: str,
    source_label: str,
) -> None:
    previous = translations.get(offset)
    if previous is not None and previous != value:
        raise SoftpalToolError(
            f"偏移 0x{offset:X} 出现冲突翻译，来源 {source_label} 与之前值不一致"
        )
    translations[offset] = value


def collect_translations(
    translation_items: list[dict[str, Any]],
    records_by_offset: dict[int, TextRecord],
) -> dict[int, str]:
    translations: dict[int, str] = {}

    for item in translation_items:
        if not isinstance(item, dict):
            continue

        if "offset" in item:
            offset = item.get("offset")
            if isinstance(offset, int) and offset in records_by_offset:
                record = records_by_offset[offset]
                translated = translated_field(item, "text", record.text)
                if translated is not None:
                    add_translation(translations, offset, translated, f"text_records[0x{offset:X}]")

        message_offset = item.get("message_offset")
        if isinstance(message_offset, int) and message_offset in records_by_offset:
            original = records_by_offset[message_offset].text
            translated = translated_field(item, "message", original)
            if translated is not None:
                add_translation(
                    translations,
                    message_offset,
                    translated,
                    f"dialogue.message[0x{message_offset:X}]",
                )

        name_offset = item.get("name_offset")
        if isinstance(name_offset, int) and name_offset in records_by_offset:
            original = records_by_offset[name_offset].text
            translated = translated_field(item, "name", original)
            if translated is not None:
                add_translation(
                    translations,
                    name_offset,
                    translated,
                    f"dialogue.name[0x{name_offset:X}]",
                )

    return translations


def collect_dialogue_text_offsets(dialogue_entries: list[DialogueEntry]) -> set[int]:
    offsets: set[int] = set()
    for entry in dialogue_entries:
        for value in (entry.message_offset, entry.name_offset):
            if value not in (None, -1, 0x0FFFFFFF, 0xFFFFFFFF):
                offsets.add(value)
    return offsets


def summarize_offsets(offsets: list[int], limit: int = 8) -> str:
    shown = ", ".join(f"0x{offset:X}" for offset in offsets[:limit])
    if len(offsets) > limit:
        shown += f", ... (+{len(offsets) - limit} more)"
    return shown


def build_reinsert_warnings(
    translations: dict[int, str],
    dialogue_entries: list[DialogueEntry],
    patch_all_push_offsets: bool,
) -> list[str]:
    if patch_all_push_offsets or not translations:
        return []

    dialogue_offsets = collect_dialogue_text_offsets(dialogue_entries)
    changed_non_dialogue_offsets = sorted(
        offset for offset in translations if offset not in dialogue_offsets
    )
    if not changed_non_dialogue_offsets:
        return []

    return [
        "当前未启用 --patch-all-push-offsets，但检测到修改了不在已识别主对话 "
        f"name/message 集合内的 TEXT.DAT 记录: {summarize_offsets(changed_non_dialogue_offsets)}。"
        "默认 dialogue_only 模式只修补已识别主对话里的 name/message 偏移；"
        "这类改动会改变后续记录的文件内偏移，可能导致其它文本调用仍沿用旧偏移而出现缺字、错位或乱码。"
        "建议改用 --patch-all-push-offsets 重新回注。"
    ]


def rebuild_text_dat(
    source_header: bytes,
    records: list[TextRecord],
    translations: dict[int, str],
    encode_body: bool,
) -> tuple[bytes, dict[int, int], int]:
    if len(source_header) != TEXT_HEADER_SIZE:
        raise SoftpalToolError("TEXT.DAT 头部长度不对")

    header = bytearray(source_header)
    p32(header, 0x0C, len(records))
    rebuilt = bytearray(header)
    offset_map: dict[int, int] = {}
    changed_count = 0

    for record in records:
        offset_map[record.offset] = len(rebuilt)
        text_bytes = record.raw_bytes
        translated = translations.get(record.offset)
        if translated is not None:
            text_bytes = encode_cp932(translated)
            changed_count += 1
        rebuilt.extend(struct.pack("<I", record.meta))
        rebuilt.extend(text_bytes)
        rebuilt.append(0)

    if encode_body:
        rebuilt = bytearray(rebuilt[:TEXT_HEADER_SIZE] + softpal_transform_encode(bytes(rebuilt[TEXT_HEADER_SIZE:])))

    return bytes(rebuilt), offset_map, changed_count


def patch_script_dialogue_offsets(
    script: bytes,
    dialogue_entries: list[DialogueEntry],
    offset_map: dict[int, int],
) -> tuple[bytes, int]:
    patched = bytearray(script)
    patched_count = 0
    touched_immediates: set[int] = set()

    for entry in dialogue_entries:
        for field_name, push_offset in (
            ("message_offset", entry.message_push_offset),
            ("name_offset", entry.name_push_offset),
        ):
            old_offset = getattr(entry, field_name)
            if push_offset is None or old_offset is None:
                continue
            if old_offset not in offset_map:
                continue
            immediate_offset = push_offset + 4
            if immediate_offset in touched_immediates:
                continue
            current_value = u32(patched, immediate_offset)
            if current_value != old_offset:
                continue
            new_offset = offset_map[old_offset]
            if new_offset != current_value:
                p32(patched, immediate_offset, new_offset)
                patched_count += 1
            touched_immediates.add(immediate_offset)

    return bytes(patched), patched_count


def patch_all_push_immediates(
    script: bytes,
    offset_map: dict[int, int],
) -> tuple[bytes, int]:
    patched = bytearray(script)
    patched_count = 0
    offset = SCRIPT_HEADER_SIZE
    size = len(script)

    while offset + 4 <= size:
        opcode = u32(patched, offset)
        if opcode == IMMEDIATE_OPCODE and offset + 8 <= size:
            immediate_offset = offset + 4
            current_value = u32(patched, immediate_offset)
            new_value = offset_map.get(current_value)
            if new_value is not None and new_value != current_value:
                p32(patched, immediate_offset, new_value)
                patched_count += 1
            offset += 8
            continue

        if opcode == 0x10017 and offset + 12 <= size:
            offset += 12
            continue

        if opcode in SCRIPT_IMMEDIATE_OPS and offset + 8 <= size:
            offset += 8
            continue

        offset += 4

    return bytes(patched), patched_count


def reinsert_command(args: argparse.Namespace) -> int:
    text_path = Path(args.text)
    script_path = Path(args.script)
    translation_json_path = Path(args.translation_json)
    out_dir = Path(args.out)

    normalized_text, records = load_text_records(text_path, decode_body=args.decode_text_body)
    records_by_offset = {record.offset: record for record in records}
    script = load_script(script_path)
    dialogue_entries, _, unresolved = extract_dialogue_entries(script, records_by_offset)

    payload = json.loads(translation_json_path.read_text(encoding="utf-8"))
    if not isinstance(payload, list):
        raise SoftpalToolError("翻译 JSON 顶层必须是数组")

    translations = collect_translations(payload, records_by_offset)
    warnings = build_reinsert_warnings(
        translations,
        dialogue_entries,
        args.patch_all_push_offsets,
    )
    rebuilt_text, offset_map, changed_count = rebuild_text_dat(
        normalized_text[:TEXT_HEADER_SIZE],
        records,
        translations,
        encode_body=args.encode_text_body,
    )

    if args.patch_all_push_offsets:
        rebuilt_script, patched_script_count = patch_all_push_immediates(script, offset_map)
        patch_mode = "all_push_offsets"
    else:
        rebuilt_script, patched_script_count = patch_script_dialogue_offsets(script, dialogue_entries, offset_map)
        patch_mode = "dialogue_only"

    out_dir.mkdir(parents=True, exist_ok=True)
    out_text = out_dir / "TEXT.DAT"
    out_script = out_dir / "SCRIPT.SRC"
    out_text.write_bytes(rebuilt_text)
    out_script.write_bytes(rebuilt_script)

    summary = {
        "changed_record_count": changed_count,
        "patched_script_immediate_count": patched_script_count,
        "patch_mode": patch_mode,
        "translation_source": str(translation_json_path),
        "decode_text_body": args.decode_text_body,
        "encode_text_body": args.encode_text_body,
        "unresolved_text_call_count_during_scan": len(unresolved),
        "warnings": warnings,
    }
    write_json(out_dir / "reinsert_summary.json", summary)

    for warning in warnings:
        print(f"[warning] {warning}", file=sys.stderr)
    print(f"[reinsert] 改动记录数: {changed_count}")
    print(f"[reinsert] SCRIPT.SRC 修补立即数: {patched_script_count}")
    print(f"[reinsert] 模式: {patch_mode}")
    print(f"[reinsert] 输出目录: {out_dir}")
    return 0


def build_argparser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Softpal 文本导出/回注工具，适配当前样本里的 TEXT.DAT + SCRIPT.SRC。"
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    extract = subparsers.add_parser("extract", help="导出 TEXT.DAT 全量记录和主对话 name-msg JSON")
    extract.add_argument("--text", required=True, help="TEXT.DAT 路径")
    extract.add_argument("--script", required=True, help="SCRIPT.SRC 路径")
    extract.add_argument("--out", required=True, help="输出目录")
    extract.add_argument(
        "--decode-text-body",
        action="store_true",
        help="先对 TEXT.DAT 的 +0x10 主体执行一次 sub_482890 逆向解码后再解析",
    )
    extract.set_defaults(func=extract_command)

    reinsert = subparsers.add_parser("reinsert", help="按翻译 JSON 重建 TEXT.DAT，并同步补丁 SCRIPT.SRC")
    reinsert.add_argument("--text", required=True, help="原始 TEXT.DAT 路径")
    reinsert.add_argument("--script", required=True, help="原始 SCRIPT.SRC 路径")
    reinsert.add_argument("--translation-json", required=True, help="翻译后的 JSON 路径")
    reinsert.add_argument("--out", required=True, help="输出目录")
    reinsert.add_argument(
        "--decode-text-body",
        action="store_true",
        help="输入 TEXT.DAT 先按 sub_482890 解码后再处理",
    )
    reinsert.add_argument(
        "--encode-text-body",
        action="store_true",
        help="写出 TEXT.DAT 时，对 +0x10 主体按 sub_482890 的逆向编码重新封装",
    )
    reinsert.add_argument(
        "--patch-all-push-offsets",
        action="store_true",
        help="激进模式: 修补脚本里所有 0x1001F immediate 且命中旧文本偏移的位置",
    )
    reinsert.set_defaults(func=reinsert_command)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_argparser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except SoftpalToolError as exc:
        print(f"[error] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
