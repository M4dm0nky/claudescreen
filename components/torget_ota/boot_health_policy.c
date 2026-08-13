#include "boot_health_policy.h"

tg_health_decision tg_boot_health_decide(const tg_boot_health *health,
                                         int64_t now_us) {
  /* En icke-väntande avbild har inget rollbackbeslut kvar att fatta:
   * esp_ota_mark_* får bara anropas för ESP_OTA_IMG_PENDING_VERIFY,
   * så en stabil boot svarar alltid WAIT hur sen pollen än är. */
  if (!health || !health->pending_verify) return TG_HEALTH_WAIT;

  int64_t elapsed_us = now_us - health->started_at_us;
  if (elapsed_us < TG_HEALTH_MIN_UPTIME_US) return TG_HEALTH_WAIT;

  /* Den framtvingade felinjektionen (fysiskt rollbacktest) faller vid
   * minimigränsen även när alla bevis finns — hela poängen är att se
   * bootloadern byta lucka på en i övrigt frisk avbild. */
  if (health->forced_failure) return TG_HEALTH_ROLLBACK;

  if ((health->passed & TG_HEALTH_REQUIRED) == TG_HEALTH_REQUIRED) {
    /* Fulla bevis accepterar även efter deadline: en sen poll får
     * aldrig rulla tillbaka en avbild som redan hunnit bli frisk. */
    return TG_HEALTH_ACCEPT;
  }
  if (elapsed_us >= TG_HEALTH_DEADLINE_US) return TG_HEALTH_ROLLBACK;
  return TG_HEALTH_WAIT;
}
