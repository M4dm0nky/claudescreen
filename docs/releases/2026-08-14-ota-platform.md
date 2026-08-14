# Release: OTA-plattformen (2026-08-14)

En dag, en båge: från en fryst panel i gryningen till en skärm som
uppdaterar sig själv över luften, ber om lov med stora knappar på glaset,
och rullar tillbaka dåliga avbilder på egen hand.

## Höjdpunkter

- **Frysen dömd och fixad.** Vibbes RAM-last + OTA-grundens bootlast
  svälte panelflushens DMA-buffert (largest block < 11 520 B ⇒ evig
  NO_MEM). Vibbe är nu uttrycklig opt-in (`-DTORGET_WITH_BUDDY=ON`),
  speglad i simulatorn, pinnad av test.
- **A/B-OTA över WiFi, bevisad i tio skarpa leveranser.** KEY3-håll eller
  UPDATE-tryck öppnar ett tiominutersfönster; token + SHA-256 + metadata-
  grind vaktar; avbilden landar i inaktiva luckan; hälsogrinden godkänner
  inom 15 s eller bootloadern rullar tillbaka. USB-C skrivs aldrig av en
  OTA och förblir räddningsväg.
- **UPDATE READY-notisen.** Tokenservern annonserar nyaste bygget på den
  befintliga kvotpollen; skärmen tar över med rubrik, väntande version
  och två stora knappar — UPDATE NOW / LATER. Timtjat tills installerad,
  tystnad när versionerna matchar, aldrig takeover mitt i en pågående
  uppdatering.
- **OTA-ringen.** Fönstret (READY, medurs dränerande äggklocka + mm:ss),
  INSTALLING (medurs fyllnad + procent + inkommande version), VERIFYING
  (SHA-256), RESTARTING. Fysiskt granskad och justerad i tre varv.
- **Bootskärmen.** VIBEPULSE-märket och WIFI → TIME → DATA som tänds av
  sina riktiga signaler; river sig när första hämtningen landat. Ingen
  NO DATA-blink vid strömstart. Fysiskt godkänd ("de lyser upp när de är
  klara").
- **Skottsäkrad tokenserver.** Döda OAuth-tokens skickas aldrig om;
  maskinvitt problås (max EN prober per Mac); 429-straffrutan persisteras
  över omstarter; used-today räknas ur cachade cykeltider och överlever
  mörkläggningar; Fable-poolen visas även när den inte är bindande gräns;
  15 anrop/h bastakt.
- **Avsändargrindar.** Binärvalet sker i sändögonblicket, versionen läses
  ur avbildens egen appbeskrivning och deklareras, -dirty-byggen vägras
  utan TG_OTA_ALLOW_DIRTY=1 — läxan från spökbinären som frös glaset.
- **Attention-larmen andas i 45 s** (4,8 s missades i praktiken);
  DONE-kort pulserar hela sitt synlighetsfönster.
- **Dokumentation.** `docs/ota.md` (hela loopen), README-avsnitt,
  agent-runbokens symptomtabell utökad med dagens fällor, CLAUDE.md-regler.

## Verifierat på den fysiska enheten

Tio OTA-leveranser (inkl. första touch-samtyckta och första helt kedjade),
PENDING_VERIFY-boot med aktiv hälsogrind och godkännande vid 9,1 s
(konsolloggad), automatisk fönster-återöppning efter OTA, rollback-nät
armerat, boot- och OTA-skärmarna granskade på glas.

## Kända kvarstående

- Värdesidan (sjunde sidan) omarbetas — ingår inte i denna release.
- CI-brygga i pusharen (vägra byggen utan grön CI) — beställd, nästa pass.
- Ringens andningspuls väntar på sin rörelsegrind.
- Pusher-daemon så UPDATE-trycket alltid har en leverantör.
