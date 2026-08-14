Read `README.md` for the repository structure and build workflow.

Setting this repo up for someone (secrets, build, flash, tokenserver)? Follow
`docs/agent-setup.md` — step-by-step, with verifications and a symptom→fix
table. Never flash the board without the user explicitly asking you to.

## Over-the-air updates

Day-to-day firmware goes over the air: `idf.py build && tools/ota-flash.sh`
(device IP from git-ignored `.ota-device`). The full loop, consent model and
troubleshooting live in `docs/ota.md` — read it before touching anything
OTA. Non-negotiables: the maintenance window opens ONLY from the device (a
3 s KEY3 hold, or the UPDATE pill on the takeover) — never claim or imply a
script can; the sender gates (newest-binary-at-send, version printed,
-dirty refused) exist because a stale archived build once froze the panel —
never bypass them with TG_OTA_ALLOW_DIRTY without the user saying so; and
after editing `tools/tokenserver/`, restart the launchd service
(`launchctl kickstart -k gui/$(id -u)/se.torget.tokenserver`) — the running
process keeps old code and the panel honestly shows the gap.

## AMOLED visual work

Use `.claude/skills/iterating-esp32-amoled-ui/SKILL.md` for AMOLED work. Show
exact 480 x 480 output at meaningful stages. Review the static physical AMOLED
before motion. Studio approval never authorizes a flash; obtain explicit user
authorization for the physical install.

## Hardware-aware work

Before proposing external hardware, declaring a device limitation, or designing
a hardware-dependent feature, read `spec/hardware.md`,
`spec/hardware-capabilities.yaml`, `spec/hardware-sources.yaml`,
`spec/device-units.yaml`, and `spec/hardware-opportunities.md`. State whether
the idea is only silicon-capable, board-wired, firmware-enabled, and
physically verified on the named unit. Mention a relevant
unused onboard capability when it materially improves the request.
Never copy secrets or turn an opportunity into authorized implementation work.
