#!/bin/sh
# Skjut en byggd Torget-avbild över luften. Arbetsflödet (2026-08-14):
#
#   1. idf.py build                       (eller -B <byggkatalog>)
#   2. tools/ota-flash.sh <enhetens-ip>   — skriptet väntar på fönstret
#   3. Håll KEY ~3 s tills UPDATES ON-ringen syns — uppladdningen går av
#      sig själv, enheten verifierar SHA-256, byter lucka och startar om.
#   4. Efter omstarten återöppnas fönstret själv (PENDING_VERIFY-boot):
#      nästa bygge i samma arbetspass behöver inget nytt håll. Ett kort
#      KEY-tryck stänger när passet är klart.
#
# Samtyckeskedjan är avsiktlig: fysiskt håll + token + tidsbegränsat
# fönster. Skriptet kan aldrig öppna fönstret åt dig — det är poängen.
# USB-C förblir räddningsvägen om en avbild inte bootar.
set -eu
cd "$(dirname "$0")/.."

# IP:t kan bo i gitignorade .ota-device (repytroten) — då räcker
# `tools/ota-flash.sh` utan argument, från vilken agent-session som helst.
HOST=${1:-$(cat .ota-device 2>/dev/null || true)}
[ -n "$HOST" ] || {
  echo "usage: tools/ota-flash.sh <device-ip> [build-dir]" >&2
  echo "       (eller skriv enhetens IP i .ota-device i repytroten)" >&2
  exit 1
}
TOKEN=$(sed -n 's/.*TG_OTA_TOKEN[^"]*"\([0-9a-f]\{64\}\)".*/\1/p' secrets.h | head -1)
[ -n "$TOKEN" ] || { echo "inget TG_OTA_TOKEN i secrets.h — uppladdning avstängd" >&2; exit 1; }
BUILD_ARG=${2:-}

echo "väntar på underhållsfönstret på $HOST — håll KEY ~3 s..."
while ! curl -s --max-time 2 "http://$HOST/api/ota/status" 2>/dev/null \
    | grep -q '"maintenance_open":true'; do
  sleep 1
done

# Filvalet sker HÄR, i uppladdningsögonblicket: en hårdkodad "build" (och
# ett val vid skriptstart, medan ett bygge ännu inte skrivit sin bin)
# sköt morgonens frysspöke till glaset 2026-08-14. Nyaste build*/torget.bin
# NU vinner, och namnet skrivs ut så avsändaren ser vad som faktiskt går.
BUILD=${BUILD_ARG:-$(ls -t build*/torget.bin 2>/dev/null | head -1 | xargs dirname 2>/dev/null)}
[ -n "$BUILD" ] || { echo "ingen build*/torget.bin — bygg först" >&2; exit 1; }
BIN="$BUILD/torget.bin"
[ -f "$BIN" ] || { echo "hittar inte $BIN — bygg först (idf.py build)" >&2; exit 1; }
SHA=$(shasum -a 256 "$BIN" | cut -d' ' -f1)

# Avsändargrinden (läxan 2026-08-14, då ett arkiverat -dirty-diagnosbygge
# gick till glaset och frös det): versionen läses ur BINÄRENS egen
# appbeskrivning och visas ALLTID; ett -dirty-bygge vägras utan uttryckligt
# TG_OTA_ALLOW_DIRTY=1. Enhetens grindar ser bara "en giltig avbild" —
# att den är RÄTT avbild är avsändarens ansvar, och nu även skriptets.
BIN_VERSION=$(dd if="$BIN" bs=1 skip=48 count=32 2>/dev/null | tr -d '\0')
echo "avbildens version: ${BIN_VERSION:-okänd}"
case "$BIN_VERSION" in
  *-dirty*)
    if [ "${TG_OTA_ALLOW_DIRTY:-0}" != "1" ]; then
      echo "VÄGRAR: $BIN_VERSION är ett -dirty-bygge." >&2
      echo "Committa först, eller kör TG_OTA_ALLOW_DIRTY=1 om du menar det." >&2
      exit 1
    fi
    echo "(-dirty släppt igenom av TG_OTA_ALLOW_DIRTY=1)"
    ;;
esac

# STALE-GRINDEN (OBS-32, mätt 2026-08-18): -dirty-grinden ovan litar på
# versionssträngen i binären — men den beräknas när CMake KONFIGURERAR och
# cachas sedan. Ett bygge ur ett smutsigt träd bär därför ett RENT namn om
# trädet var rent vid senaste omkonfigurering, och grinden vinkar igenom
# exakt det den finns för att stoppa. Den dagen gick vår egen okommitterade
# knappfix iväg under namnet på commiten före den. Jämför mot trädet HÄR,
# vid sändningen: skiljer de sig är avbilden inte vad den utger sig för.
TREE_VERSION=$(git describe --tags --dirty 2>/dev/null || true)
if [ -n "$TREE_VERSION" ] && [ "$TREE_VERSION" != "$BIN_VERSION" ]; then
  if [ "${TG_OTA_ALLOW_STALE:-0}" != "1" ]; then
    echo "VÄGRAR: avbilden säger '$BIN_VERSION', trädet säger '$TREE_VERSION'." >&2
    echo "        Binären är byggd ur ett annat träd än det du står i — bygg om" >&2
    echo "        (idf.py build), eller kör TG_OTA_ALLOW_STALE=1 om du menar det." >&2
    exit 1
  fi
  echo "(avvikelse binär '$BIN_VERSION' mot träd '$TREE_VERSION' släppt igenom"
  echo " av TG_OTA_ALLOW_STALE=1)"
fi

# CI-BRYGGAN (beställd 2026-08-14, samma kväll som spökbinären): bara
# byggen vars commit har GRÖN CI får gå till glaset. Hashen tas ur
# versionssträngen (gXXXXXXX); en taggad version löses via git. Kräver
# nät + gh — offline-nödfall övermanas med TG_OTA_ALLOW_NO_CI=1.
if [ "${TG_OTA_ALLOW_NO_CI:-0}" != "1" ]; then
  case "$BIN_VERSION" in
    *-g*) BIN_COMMIT=${BIN_VERSION##*-g} ;;
    *)    BIN_COMMIT=$(git rev-parse --short "$BIN_VERSION" 2>/dev/null || true) ;;
  esac
  # gh:s --commit kräver FULL sha (kort hash ger tyst tom lista — mätt
  # 2026-08-14); rev-parse expanderar och bevisar samtidigt att committen
  # finns i det här trädet — en främmande binär vägras på köpet.
  BIN_COMMIT=$(git rev-parse "${BIN_COMMIT:-INGEN}" 2>/dev/null || true)
  if [ -z "${BIN_COMMIT:-}" ]; then
    echo "VÄGRAR: kan inte utläsa commit ur '$BIN_VERSION' — CI går inte att bevisa." >&2
    echo "        TG_OTA_ALLOW_NO_CI=1 om du menar det." >&2
    exit 1
  fi
  # REPOT NAMNGES UTTRYCKLIGEN (mätt 2026-08-18): `gh run list` utan -R löser
  # upp repot ur gh:s egen kontext, och utan satt default-repo kan den svara
  # för ett HELT ANNAT repo. Då rapporterade bryggan "ingen grön CI" för en
  # commit med två gröna körningar, och enda synliga vägen vidare var att
  # stänga av grinden. En grind som säger nej för att den frågade fel ställe
  # lär användaren att kringgå den. Origin är sanningen, inte omgivningen.
  REPO=$(git remote get-url origin 2>/dev/null \
         | sed -e 's#^git@github\.com:#https://github.com/#' \
               -e 's#^https://github\.com/##' -e 's#\.git$##' || true)
  if [ -z "${REPO:-}" ]; then
    echo "VÄGRAR: hittar inget origin att fråga GitHub om." >&2
    echo "        TG_OTA_ALLOW_NO_CI=1 om du menar det." >&2
    exit 1
  fi
  CI_GREEN=$(gh run list -R "$REPO" --commit "$BIN_COMMIT" --status success     --json databaseId --jq length 2>/dev/null || echo X)
  if [ "$CI_GREEN" = "X" ]; then
    echo "VÄGRAR: kan inte fråga GitHub om CI för $BIN_COMMIT i $REPO (offline? gh?)." >&2
    echo "        TG_OTA_ALLOW_NO_CI=1 om du menar det." >&2
    exit 1
  fi
  if [ "${CI_GREEN:-0}" -lt 1 ]; then
    CI_PENDING=$(gh run list -R "$REPO" --commit "$BIN_COMMIT" --json status       --jq '[.[]|select(.status!="completed")]|length' 2>/dev/null || echo 0)
    if [ "${CI_PENDING:-0}" -ge 1 ]; then
      echo "VÄGRAR: CI för $BIN_COMMIT kör fortfarande — vänta på grönt." >&2
    else
      echo "VÄGRAR: ingen grön CI för $BIN_COMMIT (opushad? röd?)." >&2
    fi
    echo "        TG_OTA_ALLOW_NO_CI=1 om du menar det." >&2
    exit 1
  fi
  echo "CI grön för $BIN_COMMIT i $REPO ($CI_GREEN körning(ar))"
fi

echo "fönstret öppet — laddar upp $BIN ($(wc -c < "$BIN" | tr -d ' ') byte):"
curl -s --max-time 300 -X POST "http://$HOST/api/ota/firmware" \
  -H "Authorization: Bearer $TOKEN" \
  -H "X-VibePulse-Project: torget" \
  -H "X-VibePulse-Chip: esp32s3" \
  -H "X-VibePulse-SHA256: $SHA" \
  --data-binary "@$BIN" \
  -w "\nHTTP %{http_code} på %{time_total}s\n"
echo "202 = avbilden vald för nästa boot; enheten startar om inom ett par sekunder."
