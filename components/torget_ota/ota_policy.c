#include "ota_policy.h"

#include <string.h>

/* Ordningen är en del av kontraktet: stängt fönster och autentisering
 * avgörs före all metadata, så ett oautentiserat anrop aldrig kan sondera
 * vilket projekt, chip eller vilken storlek enheten skulle acceptera. */
tg_ota_decision tg_ota_request_check(const tg_ota_request *r) {
  if (!r || !r->maintenance_open) return TG_OTA_REJECT_CLOSED;
  if (!r->authorized) return TG_OTA_REJECT_AUTH;
  if (!r->project || strcmp(r->project, "torget") != 0)
    return TG_OTA_REJECT_PROJECT;
  if (!r->chip || strcmp(r->chip, "esp32s3") != 0)
    return TG_OTA_REJECT_CHIP;
  if (r->content_length == 0 || r->content_length > TG_OTA_MAX_IMAGE_BYTES ||
      r->content_length > r->slot_size)
    return TG_OTA_REJECT_SIZE;
  return TG_OTA_ACCEPT;
}

/* 100 lovar att hela kroppen är mottagen — en delvis mottagen bild
 * trunkeras därför nedåt och kan aldrig avrundas upp till löftet. */
unsigned tg_ota_progress_percent(size_t received, size_t total) {
  if (!total) return 0;
  if (received >= total) return 100;
  return (unsigned)((received * 100U) / total);
}
