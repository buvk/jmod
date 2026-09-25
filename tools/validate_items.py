#!/usr/bin/env python3
from __future__ import annotations

from collections import Counter
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ITEM_NAMES_PATH = ROOT / "src" / "item_names.h"
LOOT_FILTER_PATH = ROOT / "loot_filter.ini"
EXPECTED_ITEM_COUNT = 647
ALLOWED_LEGACY_SECTIONS = {None, "Weapons", "Armor"}

ITEM_RE = re.compile(
    r'^\s*\{\s*(0x[0-9A-Fa-f]+)[uUlL]*\s*,\s*"([^"]+)"\s*,\s*'
    r'"([^"]+)"\s*,\s*(NULL|"[^"]+")\s*,\s*([01])\s*\},\s*$'
)
SECTION_RE = re.compile(r'^\[([^\]]+)\]$')
MASK_RE = re.compile(r'^(?:0[xX][0-9A-Fa-f]+|0[0-7]*|[1-9][0-9]*|0)$')


def error(errors: list[str], message: str) -> None:
    errors.append(f"ERROR: {message}")


def parse_item_names(errors: list[str]) -> list[dict[str, object]]:
    entries: list[dict[str, object]] = []
    in_table = False
    found_table = False
    lines = ITEM_NAMES_PATH.read_text(encoding="utf-8").splitlines()

    for line_no, raw in enumerate(lines, 1):
        line = raw.strip()
        if not in_table:
            if re.fullmatch(r"static\s+const\s+ItemName\s+item_names\[\]\s*=\s*\{", line):
                in_table = True
                found_table = True
            continue

        if line == "};":
            in_table = False
            break
        if not line or line.startswith(("/*", "*", "*/", "//")):
            continue

        match = ITEM_RE.match(raw)
        if not match:
            error(errors, f"{ITEM_NAMES_PATH.relative_to(ROOT)}:{line_no}: unrecognized item entry")
            continue
        code_text, name, section, legacy, quest_text = match.groups()
        entries.append(
            {
                "code": int(code_text, 16),
                "code_text": code_text,
                "name": name,
                "section": section,
                "legacy": None if legacy == "NULL" else legacy[1:-1],
                "quest": quest_text == "1",
                "line": line_no,
            }
        )

    if not found_table:
        error(errors, f"item_names array not found in {ITEM_NAMES_PATH.relative_to(ROOT)}")
    elif in_table:
        error(errors, f"unterminated item_names array in {ITEM_NAMES_PATH.relative_to(ROOT)}")
    elif not entries:
        error(errors, f"no item definitions found in {ITEM_NAMES_PATH.relative_to(ROOT)}")
    return entries


def parse_c_mask(value: str) -> int | None:
    if not MASK_RE.fullmatch(value):
        return None
    if value.lower().startswith("0x"):
        return int(value[2:], 16)
    if len(value) > 1 and value.startswith("0"):
        return int(value, 8)
    return int(value, 10)


def parse_loot_filter(errors: list[str]) -> list[dict[str, object]]:
    entries: list[dict[str, object]] = []
    section: str | None = None
    for line_no, raw in enumerate(LOOT_FILTER_PATH.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        match = SECTION_RE.fullmatch(line)
        if match:
            section = match.group(1)
            continue
        if "=" not in raw:
            error(errors, f"{LOOT_FILTER_PATH.name}:{line_no}: expected section or key=value entry")
            continue
        if section is None:
            error(errors, f"{LOOT_FILTER_PATH.name}:{line_no}: item appears before any section")
            continue
        name, value = raw.split("=", 1)
        name = name.strip()
        value = value.strip()
        if not name:
            error(errors, f"{LOOT_FILTER_PATH.name}:{line_no}: empty item name")
            continue
        mask = parse_c_mask(value)
        if mask is None or mask > 0x3F:
            error(
                errors,
                f'{LOOT_FILTER_PATH.name}:{line_no}: invalid mask "{value}" for "{name}"; expected 0x00-0x3F',
            )
        entries.append(
            {
                "name": name,
                "section": section,
                "mask": mask,
                "line": line_no,
            }
        )
    return entries


def report_duplicates(errors: list[str], label: str, values: list[object]) -> None:
    duplicates = [value for value, count in Counter(values).items() if count > 1]
    for value in duplicates:
        error(errors, f"duplicate {label}: {value!r}")


def main() -> int:
    errors: list[str] = []
    header = parse_item_names(errors)
    ini = parse_loot_filter(errors)

    codes = [entry["code"] for entry in header]
    names = [entry["name"] for entry in header]
    report_duplicates(errors, "item code", codes)
    report_duplicates(errors, "item name", names)

    if len(header) != EXPECTED_ITEM_COUNT:
        error(
            errors,
            f"item_names.h item count changed: expected {EXPECTED_ITEM_COUNT}, found {len(header)}",
        )
    if len(ini) != EXPECTED_ITEM_COUNT:
        error(
            errors,
            f"loot_filter.ini item count changed: expected {EXPECTED_ITEM_COUNT}, found {len(ini)}",
        )

    for previous, current in zip(header, header[1:]):
        if int(previous["code"]) >= int(current["code"]):
            error(
                errors,
                "item_names.h is not strictly sorted by code: "
                f'{previous["code_text"]} {previous["name"]!r} (line {previous["line"]}) '
                f'appears before {current["code_text"]} {current["name"]!r} (line {current["line"]})',
            )

    for entry in header:
        if entry["legacy"] not in ALLOWED_LEGACY_SECTIONS:
            error(
                errors,
                f'{ITEM_NAMES_PATH.relative_to(ROOT)}:{entry["line"]}: invalid legacy section '
                f'{entry["legacy"]!r} for {entry["name"]!r}',
            )
        is_quest_section = entry["section"] == "Quest Items"
        if bool(entry["quest"]) != is_quest_section:
            error(
                errors,
                f'{ITEM_NAMES_PATH.relative_to(ROOT)}:{entry["line"]}: quest flag/section mismatch for '
                f'{entry["name"]!r}: section={entry["section"]!r}, quest={int(bool(entry["quest"]))}',
            )

    ini_names = [entry["name"] for entry in ini]
    report_duplicates(errors, "loot_filter.ini item", ini_names)

    header_by_name = {str(entry["name"]): entry for entry in header}
    ini_by_name = {str(entry["name"]): entry for entry in ini}

    for name in sorted(header_by_name.keys() - ini_by_name.keys()):
        entry = header_by_name[name]
        error(errors, f'{LOOT_FILTER_PATH.name}: missing item {name!r} from section {entry["section"]!r}')

    for name in sorted(ini_by_name.keys() - header_by_name.keys()):
        entry = ini_by_name[name]
        error(
            errors,
            f'{LOOT_FILTER_PATH.name}:{entry["line"]}: unexpected item {name!r} in section {entry["section"]!r}',
        )

    for name in sorted(header_by_name.keys() & ini_by_name.keys()):
        header_entry = header_by_name[name]
        ini_entry = ini_by_name[name]
        if header_entry["section"] != ini_entry["section"]:
            error(
                errors,
                f'section mismatch for {name!r}: item_names.h={header_entry["section"]!r}, '
                f'loot_filter.ini={ini_entry["section"]!r} (line {ini_entry["line"]})',
            )

    if len(header) != len(ini):
        error(errors, f"item count mismatch: item_names.h={len(header)}, loot_filter.ini={len(ini)}")

    if errors:
        for message in errors:
            print(message, file=sys.stderr)
        print(f"FAILED: {len(errors)} validation error(s)", file=sys.stderr)
        return 1

    print(f"OK: {len(header)} item definitions validated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
