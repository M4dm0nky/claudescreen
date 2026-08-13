#!/usr/bin/env python3
"""Partition-budget gate: two equal OTA slots, no factory, 4 MiB image cap."""

import csv
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
rows = []
for raw in (root / "partitions.csv").read_text().splitlines():
    raw = raw.strip()
    if raw and not raw.startswith("#"):
        rows.append(next(csv.reader([raw], skipinitialspace=True)))

parts = {row[0].strip(): row for row in rows}
assert "factory" not in parts, "normal updates must not target a factory slot"
assert {"nvs", "otadata", "phy_init", "ota_0", "ota_1"} <= set(parts)

def size(text):
    match = re.fullmatch(r"(\d+)([KkMm]?)", text.strip())
    assert match, text
    value = int(match.group(1))
    return value * {"": 1, "k": 1024, "m": 1024 * 1024}[match.group(2).lower()]

slot0 = size(parts["ota_0"][4])
slot1 = size(parts["ota_1"][4])
assert slot0 == slot1 == 5 * 1024 * 1024
assert parts["ota_0"][2].strip() == "ota_0"
assert parts["ota_1"][2].strip() == "ota_1"
assert size(parts["otadata"][4]) == 8 * 1024
maximum_permitted = 4 * 1024 * 1024
assert maximum_permitted <= slot0
binary = root / "build/torget.bin"
if binary.exists():
    assert binary.stat().st_size <= maximum_permitted
print("OK: two 5 MiB OTA slots; 4 MiB maximum image gate")
