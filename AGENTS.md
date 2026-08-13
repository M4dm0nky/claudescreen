# Torget — appplattform för hyllskärmen

(Arbetsnamn — Niclas kan döpa om.) Torget äger den fysiska skärmen
(Waveshare ESP32-S3-Touch-AMOLED-2.16, 480×480) och producerar DEN enda
firmware-binär som flashas. Appar pluggar in som ESP-IDF-komponenter och kan
bo i egna repon. En skärm = en binär = ett bygge här. MIT-licens.

Struktur, byggkommandon och hur man skriver en app: **README.md** (läs den
först). Hårdvarusanningen routas under `Hardware-aware work` nedan; läs den
kanoniska femfilslistan där före hårdvaruarbete. Designsystemet:
**spec/ui-spec.md**. Historiken och P-numren: Solceller-repots
`docs/roadmap-hyllskarmen.md` — P25 + P25-VISION är detta repos
födelseattest.

## New here? Setting this repo up for someone

**If you were handed this repo to _set it up_** — fill in secrets, build,
flash, start the service — follow
**[docs/agent-setup.md](docs/agent-setup.md)**, not this file. It is in
English, step-by-step, with a verification after every step and a
symptom→fix table.

The rest of AGENTS.md is maintainer context for working *on* the platform.
It refers to things you do not have and do not need: `~/Buddy/components`
(app 3) and the Solceller repo's roadmap. Both builds gate on their absence
and simply build two apps instead of three.

## Status (2026-08-13, första flashen gjord)

Plattformen kopierades (flyttades inte) från `~/Documents/Solceller/firmware/`
och stöptes om enligt granskningens tre krav: (1) versionerat appkontrakt
`torget_app_t` + appregister i main/registry.c som launchern läser;
(2) nätverk/hämtning bor i varje apps egen komponent (net.c), plattformen
äger bara WiFi/SNTP/lås/ljus/rotation; (3) MIT-licens. Tre appar är
registrerade: Solelkollen (datakontraktet mot solelkollen.se oförändrat),
VibePulse (Claude/Codex-användning via tools/tokenserver på Macen, platt JSON
över LAN) och Vibbe/Buddy från companion-repots build input
`~/Buddy/components` (exakt källrevision i `spec/hardware-sources.yaml`).

**Detta repo äger nu glaset.** Den första fysiska flashen gjordes 2026-08-13
med användarens uttryckliga tillåtelse, från mergad main; den statiska
fysiska grinden är PASSERAD för kvotsidorna, agentmonitorn och Max Tracker,
och Solceller-kopian är avvecklad som skärmens drivrutin. Evidens:
`docs/superpowers/reviews/2026-08-13-max-tracker-physical-static.md`.
Fortfarande ogrindat: rörelse/animation — den kräver
interaktionsprotokollet i AMOLED-skillen, mätt på panelen först.

## Arbetsregler

- **Bänk/sim först, flash sen.** Simulatorn är specen; en obevakad körning
  dumpar /tmp/torget-*.bmp som pixelfacit.
- **Värdlagren är tunna.** platform/, components/app_* (utom net.c) och
  kärnkomponenterna delas byte-identiskt mellan sim och target. UI-beteende
  hör hemma i appen/platform/, aldrig i main/main.c eller sim/main.c.
- **Kontraktet är heligt.** Bryts torget_app.h eller torget.h bumpas
  TORGET_APP_API_VERSION — launchern hoppar över appar med fel version.
- **Biblioteksregeln:** skörda vid ANDRA användningen, bygg aldrig i förväg
  (torget_fmt, torget_ticker, torget_net skördades när VibePulse, då kallad
  Tokenmätaren, blev andra användaren — det är mallen).
- **Ärlighetsinvarianten:** aldrig påhittade nollor — utan data visas
  streck; räknare backar aldrig; eSett är AVRÄKNAD el och copyn säger det.

## AMOLED visual work

Use `.claude/skills/iterating-esp32-amoled-ui/SKILL.md` for AMOLED work. Show
exact 480 x 480 output at meaningful stages. Review the static physical AMOLED
before motion. Studio approval never authorizes a flash; obtain explicit user
authorization for the physical install.

## Hårdvarufällorna i kortform (detaljer i spec/hardware.md)

- `bsp_display_lock()` LJUGER (esp_err_t genom bool, spegelvänt) — använd
  `esp_lv_adapter_lock(-1)` direkt (plattformens torget_ui_lock gör rätt),
  annars heapkorruption och eviga loopar.
- LVGL 9.5: `lv_span_set_text` ritar INTE om — explicit `lv_spangroup_refresh`.
- För IMU-adress och fysisk verifiering gäller capability
  `sensors.imu-qmi8658`; komponentens header deklarerar `read_accel_mg`, som
  saknas i källan — använd `read_accel`.
- Rotationskalibrering: SG_QUAD_UP 1, SG_QUAD_DIR -1, trösklar med
  iterationshistorik i rotation.c. Kalibrera fysisk hårdvara med ETT
  strukturerat flerlägestest, aldrig fotoforensik; en konstant per iteration.
- S3:an är 2,4 GHz-only; bootskanningen i loggen är facit för vilka nät som
  finns. MADCTL och touch roteras ALLTID i samma grepp.
- ESP-IDF 5.5.2 i `~/esp/esp-idf` (`. ~/esp/esp-idf/export.sh`), LVGL pinnad
  9.5.0 i BÅDE sim och target (bump båda i samma commit).

## Medvetet SENARE (bygg inte förrän triggern slår)

Vibbe/Buddy är redan app 3 via companion-inputen `~/Buddy/components`.
`audio.microphones`, `audio.speaker-output` och `usb.device` är
firmware-enabled genom det build-inputet; exakt revision och evidens finns i
`spec/hardware-sources.yaml`. Fysisk mikrofon-/högtalarfunktion är fortfarande
overifierad.

- WiFi-provisionering + OTA + konfig-UI — trigger: första enheten som lämnar
  huset (kompilerad secrets.h duger inte för sålda enheter; ESP-IDF:s
  wifi_provisioning finns färdig).
- Responsiv layout för andra Waveshare-storlekar — trigger: andra skärmtypen.
- Butiks-/paketmaskineri och appar i egna repon via Espressifs registry —
  trigger: bevisad traktion efter open source.
- Röststyrning för befintliga Vibbe/Buddy är kandidat/senare tills privacy-UI,
  fysisk mikrofon-/högtalarverifiering samt full-duplex-, audio- och
  nätverksvalidering finns. Kandidatstatus är inte auktoriserat arbete;
  molntjänst och betalningsmodell är separata produktbeslut.

## Hardware-aware work

Before proposing external hardware, declaring a device limitation, or designing
a hardware-dependent feature, read `spec/hardware.md`,
`spec/hardware-capabilities.yaml`, `spec/hardware-sources.yaml`,
`spec/device-units.yaml`, and `spec/hardware-opportunities.md`. State whether
the idea is only silicon-capable, board-wired, firmware-enabled, and
physically verified on the named unit. Mention a relevant
unused onboard capability when it materially improves the request.
Never copy secrets or turn an opportunity into authorized implementation work.
