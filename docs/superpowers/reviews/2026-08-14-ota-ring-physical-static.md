# 2026-08-14 — OTA-ringen: fysisk statisk granskning

## Setup

- Build: `claude/ota-foundation` @ `5a0cb8e` (`v0.2.1-21-g5a0cb8e`), levererad
  **över luften** till enheten på väggström — sessionens tredje lyckade
  OTA-varv (`ota_0 → ota_1 → ota_0`), 202 på 14 s, hälsogrinden godkände
  live (running_partition `ota_0` efteråt, ingen rollback).
- Riktning A ("Ringen") vald ur tre 480×480-mockuper samma dag; de fyra
  lägenas simulator-raster (lv_layer_top-snapshots) granskade 1:1 och
  landmärkes-testade före flash (`test_vibepulse_visual_landmarks.py`).

## Observerat på glaset

- **UPDATES ON, statiskt: GODKÄNT.** Användarens ord: "updates on and a
  ring with clock in the centre" — lägesordet överst, ringen mitt på
  glaset, mm:ss-klockan (plex_num_84) i centrum. Ingen rapport om
  klippning eller feljustering.

## Inte observerat (ärlig avgränsning)

- RECEIVING/VERIFYING/RESTARTING på glas: uppladdningen var klar på ~14 s
  och ingen bekräftelse finns på att förloppet hann iakttas. Rastren är
  sim-bevisade; nästa OTA på väggström är ett naturligt granskningstillfälle.
- Återöppningen efter omstart sågs inte uttryckligen (fönstret var stängt
  när statusendpointen pollades ~2 min efter boot; ett kort KEY3-tryck är
  den designade stängningen och den troliga förklaringen). Grindvillkoret
  är värdtestat (`test_ota_reopen_wiring.py`).

## Nästa grind

- RÖRELSE (andningspunkten på bågens huvud, ev. dränerings-tick) kräver en
  EGEN granskning efter denna statiska — instrumentera den delade
  ritpipen enligt skillens mätprotokoll innan någon animation landar.
