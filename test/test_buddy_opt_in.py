#!/usr/bin/env python3
"""Regression guard: Vibbe/Buddy must stay opt-in on the VibePulse panel.

Frysläxan 2026-08-14: med Buddy ombord föll största sammanhängande
DMA-blocket under panelflushens 11 520 B (480 x 12 x 2) och varje flush dog
i ESP_ERR_NO_MEM — glaset frös på sista bilden medan systemet levde vidare,
och TinyUSB gömde konsolen exakt när den behövdes. Bygget får därför aldrig
återgå till att dra in Buddy bara för att ~/Buddy råkar vara utcheckad."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")

assert 'option(TORGET_WITH_BUDDY' in cmake and 'OFF)' in cmake.split(
    "option(TORGET_WITH_BUDDY", 1)[1].split("\n", 1)[0], (
    "Vibbe/Buddy ska vara en uttrycklig option med default OFF "
    "(-DTORGET_WITH_BUDDY=ON), inte auto-inkluderad"
)
assert 'if(TORGET_WITH_BUDDY AND EXISTS "${TORGET_BUDDY_DIR}/app_buddy")' in cmake, (
    "Buddys EXTRA_COMPONENT_DIRS-post ska grindas på TORGET_WITH_BUDDY, "
    "inte bara på att katalogen finns"
)
assert 'set(ENV{TORGET_BUDDY_DIR} "")' in cmake, (
    "Bortvald Buddy ska spegla en TOM env-sökväg så main/CMakeLists.txt "
    "aldrig ser ~/Buddy av misstag (även om skalet exporterat den)"
)
assert 'if(EXISTS "${TORGET_BUDDY_DIR}/app_buddy")' not in cmake, (
    "Den ogrindade EXISTS-inkluderingen av Buddy får inte återuppstå"
)

print("OK: Vibbe/Buddy är opt-in — panelflushens DMA-marginal skyddad")
