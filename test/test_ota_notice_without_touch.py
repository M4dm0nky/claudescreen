#!/usr/bin/env python3
"""En takeover får aldrig erbjuda en knapp den inte kan ta emot.

Läxan 2026-08-18: UPDATE READY tog hela glaset med UPDATE NOW och LATER
medan touchen inte kommit upp på den booten — ett tyst fel, för
`bsp_touch_new` får misslyckas (panelen är poängen, en bootloop är värre)
och lämnar bara en ESP_LOGE-rad. En död touchpanel ser exakt ut som en
levande. Samma dag var KEY-hållet dessutom dött av porteringsskäl, så en
frisk enhet hade ingen väg alls att säga vad som var fel.

Enheten VET vid boot om touchen registrerades. Då ska notisen namnge den
fysiska vägen ut i stället för att rita pillren."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
main_c = (root / "main" / "main.c").read_text(encoding="utf-8")
ota_ui = (root / "components" / "torget_ota" / "ota_ui.c").read_text(encoding="utf-8")
ota_ui_h = (root / "components" / "torget_ota" / "ota_ui.h").read_text(encoding="utf-8")

# Kontraktet: en uttrycklig setter, inte en global som alla får peta i.
assert "torget_ota_ui_set_touch_available" in ota_ui_h, (
    "ota_ui ska exponera touch-tillgängligheten som ett uttryckligt kontrakt"
)
assert "torget_ota_ui_set_touch_available" in ota_ui, (
    "ota_ui ska implementera settern"
)

# main.c äger sanningen: den vet om esp_lv_adapter_register_touch gav något.
assert "torget_ota_ui_set_touch_available" in main_c, (
    "main.c ska rapportera touchens verkliga utfall till overlayn"
)
touch_call = main_c.index("torget_ota_ui_set_touch_available")
create_call = main_c.index("torget_ota_ui_create()")
assert touch_call > create_call, (
    "overlayn måste finnas innan den får veta något — sätt flaggan EFTER "
    "torget_ota_ui_create()"
)

# Default = touch finns. En anropare som aldrig sätter flaggan (simulatorn,
# ett framtida kort) ska bete sig som förut, inte tappa sina pill.
assert "s_touch_available = true" in ota_ui, (
    "utan besked ska overlayn anta att touch finns — annars tappar "
    "simulatorn och varje ny anropare sina pill i tysthet"
)

# Renderingen: utan touch ritas INGA pill, och den fysiska vägen ut står
# på glaset i stället.
assert '"HOLD KEY 3S"' in ota_ui, (
    "utan touch ska notisen namna den vag som fortfarande fungerar"
)
notice_branch = ota_ui[ota_ui.index("if (state == TG_OTA_UI_NOTICE) {"):]
notice_branch = notice_branch[:notice_branch.index("} else {")]
assert "s_touch_available" in notice_branch, (
    "NOTICE-grenen ska välja pill eller tangenttext på touchens utfall"
)

print("OK: en takeover utan touch namnger tangenten i stället för att rita pill")
