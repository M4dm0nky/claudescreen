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

# --- Task 5: boot-health adapter wiring (source-level, no ESP-IDF needed) ---

main_c = (root / "main/main.c").read_text(encoding="utf-8")
adapter_path = root / "components/torget_ota/boot_health.c"
assert adapter_path.exists(), "boot_health.c must exist in torget_ota"
adapter = adapter_path.read_text(encoding="utf-8")

# The health gate only means something if the target actually starts it and
# feeds it the local evidence bits at the right places in the boot order.
assert "torget_boot_health_start();" in main_c, (
    "app_main must start the boot-health gate after NVS init"
)
assert "torget_boot_health_mark(TG_HEALTH_DISPLAY);" in main_c, (
    "display evidence must be marked after display_start()"
)
assert "torget_boot_health_mark(TG_HEALTH_UI);" in main_c, (
    "UI evidence must be marked after torget_ui_create()"
)
assert "torget_boot_health_mark(TG_HEALTH_SCHEDULER);" in main_c, (
    "scheduler evidence must be marked from the first tick_cb"
)

# Accept and rollback must both go through the official rollback API; anything
# else leaves otadata in a state the bootloader does not understand.
assert "esp_ota_mark_app_valid_cancel_rollback" in adapter, (
    "the adapter must accept only through esp_ota_mark_app_valid_cancel_rollback"
)
assert "esp_ota_mark_app_invalid_rollback_and_reboot" in adapter, (
    "the adapter must roll back through esp_ota_mark_app_invalid_rollback_and_reboot"
)
assert "ESP_OTA_IMG_PENDING_VERIFY" in adapter, (
    "policy polling must be enabled only for a pending-verify image"
)
assert "ESP_OTA_IMG_ABORTED" in adapter, (
    "a stable boot must record bootloader crash-rollback evidence from the other slot"
)
assert '"rollback_from"' in adapter and '"torget_health"' in adapter, (
    "rollback evidence must land in the bounded NVS key rollback_from"
)

# Component wiring: the adapter must be compiled into the target build and
# main must be allowed to include boot_health.h.
ota_cmake = (root / "components/torget_ota/CMakeLists.txt").read_text(encoding="utf-8")
main_cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")
assert '"boot_health.c"' in ota_cmake, "torget_ota must compile boot_health.c"
assert "torget_ota" in main_cmake, "main must require torget_ota"

# The deliberate failure injection stays a build-time development switch,
# mapped to forced_failure only for a pending image.
kconfig = (root / "main/Kconfig.projbuild").read_text(encoding="utf-8")
assert "TORGET_BOOT_HEALTH_FORCE_FAIL" in kconfig
assert "CONFIG_TORGET_BOOT_HEALTH_FORCE_FAIL" in adapter

print("OK: boot-health adapter is wired into the target boot order")
