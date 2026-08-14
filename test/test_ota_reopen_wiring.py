#!/usr/bin/env python3
"""Regression guard: the maintenance window re-arms itself only after an OTA.

Beslut 2026-08-14: den som itererar ska hålla KEY3 en gång per arbetspass,
inte en gång per bygge — men samtyckeskedjan får aldrig brytas. Återöppningen
ska därför grindas på ESP_OTA_IMG_PENDING_VERIFY (en boot som per definition
föregicks av ett fysiskt hålls uppladdning) och gå genom samma tidsbegränsade
open_maintenance som knappen. En esptool-flashad boot lämnas stängd."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
service = (root / "components" / "torget_ota" / "ota_service.c").read_text(
    encoding="utf-8")

start = service.split("void torget_ota_service_start(void)", 1)[1]

assert "ESP_OTA_IMG_PENDING_VERIFY" in start, (
    "återöppningen ska grindas på PENDING_VERIFY — den enda boot som "
    "bevisligen föregicks av ett fysiskt KEY3-håll"
)
assert "torget_ota_service_open_maintenance()" in start, (
    "återöppningen ska gå genom samma tidsbegränsade open_maintenance "
    "som knappen — ingen egen, längre lucka"
)
assert start.count("open_maintenance") == 1, (
    "exakt EN återöppning i service_start; fler vägar in urholkar samtycket"
)

print("OK: fönstret återöppnas bara efter en riktig OTA, genom samma lucka")
