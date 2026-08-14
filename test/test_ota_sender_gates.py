#!/usr/bin/env python3
"""Regression guard: the OTA pusher's sender gates must never erode.

Läxorna 2026-08-14: en hårdkodad byggkatalog och ett filval vid skriptstart
sköt en arkiverad frysbinär till glaset; -dirty-byggen och byggen utan grön
CI ska aldrig kunna levereras av misstag. Enheten bevisar att en avbild är
GILTIG — skriptet bevisar att den är RÄTT."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
script = (root / "tools" / "ota-flash.sh").read_text(encoding="utf-8")

wait_idx = script.index('"maintenance_open":true')
pick_idx = script.index("ls -t build*/torget.bin")
assert pick_idx > wait_idx, (
    "binärvalet ska ske EFTER fönsterväntan (uppladdningsögonblicket) — "
    "ett val vid skriptstart valde spökbinären medan bygget skrev sin fil"
)
assert 'BIN_VERSION=$(dd if="$BIN" bs=1 skip=48 count=32' in script, (
    "versionen ska läsas ur avbildens egen appbeskrivning och deklareras"
)
assert "TG_OTA_ALLOW_DIRTY" in script and "*-dirty*" in script, (
    "-dirty-byggen ska vägras utan uttrycklig TG_OTA_ALLOW_DIRTY=1"
)
assert "TG_OTA_ALLOW_NO_CI" in script and "gh run list --commit" in script, (
    "CI-bryggan: byggen utan grön CI för sin commit ska vägras, med "
    "TG_OTA_ALLOW_NO_CI=1 som enda nödventil"
)

ci = (root / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
assert "branches: ['**']" in ci, (
    "CI ska köra på alla brancher — annars kan bryggan aldrig se grönt "
    "för branchbyggen och blockerar det dagliga flödet"
)

print("OK: avsändargrindarna står — rätt fil, deklarerad version, ren build, grön CI")
