# Simulatorfixturer

Inspelade och konstruerade API-svar som simulatorn och hosttesterna delar.
Samma parsrar som targetet läser dem, så en payload som renderar här kan
inte felparsa på hyllan.

Fixturerna för companion-appar (Solelkollen `/api/glance`) bor i deras egna
repon tillsammans med apparna — det här repot innehåller bara VibePulse.

Regler som fixturerna låser:

- Före FÖRSTA lyckade svaret: streck, aldrig påhittade nollor.
- Efter fel: senaste goda datat kvarstår, stale-markering efter 120 s.

## Max Tracker-fixturer

Konstruerade `/api/max-tracker`-svar i dense form (kontraktet ligger i
`docs/superpowers/specs/2026-08-12-max-tracker-design.md`).

| Fil | Ursprung | Testar |
|---|---|---|
| `max-tracker-full.json` | SYNTETISK | Mogen användare: båda providers fullfärgade 140 dagar (inga `-1`), sex `[100,2]`-toppar i de senaste 6 veckorna, `weekMaxed` matchande, Codex har `planLabel`. |
| `max-tracker-coldstart.json` | SYNTETISK | Ny användare: Claude börjar med 14 dagar utan loggar, sedan gråa aktivitetsdagar (`lvl` utan `pct`), sedan 5 riktiga kvotdagar; Codex som i full. |
| `max-tracker-empty.json` | SYNTETISK | Innan första hämtningen: alla dagar `[-1,-1]`, alla aggregat `null`/0 — grafen ska rendera helt tom, aldrig påhittade nollor. |
| `max-tracker-live-shape.json` | SERVER-GENERERAD (`snapshot()` via en riktig `MaxTrackerStore` matad med brutna procent) | Regressionsfixtur: till skillnad från de tre ovan (handskrivna, alltid heltal) matas denna med Claude/Codex-utilization som den verkligen anländer — 15.5, 99.96, 0.5, 88.51 osv "by construction". Bevisar att servern avrundar till heltal INNAN serialisering (annars avvisar enhetens `int8_t`-parser hela svaret) och att en dag under 100 aldrig avrundas upp till den reserverade exakt-röda 100-cellen. |
