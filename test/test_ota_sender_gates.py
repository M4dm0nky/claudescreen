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
assert "TG_OTA_ALLOW_NO_CI" in script and "gh run list" in script, (
    "CI-bryggan: byggen utan grön CI för sin commit ska vägras, med "
    "TG_OTA_ALLOW_NO_CI=1 som enda nödventil"
)

# 2026-08-18: `gh run list` UTAN -R löser upp repot ur gh:s egen kontext, och
# utan satt default-repo kan den landa i ett ANNAT repo. Då svarar bryggan
# "ingen grön CI" för en commit som har två gröna körningar, och den enda
# vägen vidare ser ut att vara TG_OTA_ALLOW_NO_CI=1 — en grind som stängs av
# för att den frågade fel ställe är värre än ingen grind.
for call in ("gh run list", "gh api"):
    for line in script.splitlines():
        if call in line and "--commit" in line or (call in line and "--json" in line):
            assert "-R " in line or "$REPO" in line or "GH_REPO" in line, (
                f"varje {call}-anrop ska namnge repot uttryckligen: {line.strip()}"
            )
assert "git remote get-url" in script or "GH_REPO=" in script, (
    "repot ska härledas ur origin, inte ur gh:s omgivning"
)

# 2026-08-18 (OBS-32): versionssträngen i binären beräknas när CMake
# konfigurerar och cachas sedan. Ett bygge ur ett smutsigt träd kan därför
# bära ett RENT namn — då släpper -dirty-grinden igenom precis det den finns
# för att stoppa. Jämför avbildens version mot trädet vid sändningsögonblicket.
assert "git describe" in script, (
    "avbildens version ska jämföras mot trädets egen git describe --dirty; "
    "en cachad ren sträng ur ett smutsigt träd är annars osynlig"
)
assert "TG_OTA_ALLOW_STALE" in script, (
    "avvikelsen binär-mot-träd ska ha en uttrycklig nödventil, som de andra"
)

ci = (root / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
assert "branches: ['**']" in ci, (
    "CI ska köra på alla brancher — annars kan bryggan aldrig se grönt "
    "för branchbyggen och blockerar det dagliga flödet"
)

print("OK: avsändargrindarna står — rätt fil, deklarerad version, ren build, grön CI")
