#ifndef TORGET_OTA_POLICY_H
#define TORGET_OTA_POLICY_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Ren accept/avslags-policy för OTA-uppladdningar. Ingen socket, ingen
 * ESP-IDF: HTTP-gränsen översätter sin begäran till tg_ota_request och
 * frågar här INNAN esp_ota_begin får röra flashen. Därmed kan varje
 * gräns värdestestas på värddatorn utan hårdvara.
 */

/* Hårt tak under 5 MiB-luckan: partitionstestet håller samma siffra, så
 * ett bygge som växer förbi taket stoppas i CI långt innan någon försöker
 * ladda upp det. Marginalen upp till luckan är avsiktlig växtmån. */
#define TG_OTA_MAX_IMAGE_BYTES (4U * 1024U * 1024U)

typedef struct {
  bool maintenance_open;
  bool authorized;
  const char *project;
  const char *chip;
  size_t content_length;
  size_t slot_size;
} tg_ota_request;

typedef enum {
  TG_OTA_ACCEPT,
  TG_OTA_REJECT_CLOSED,
  TG_OTA_REJECT_AUTH,
  TG_OTA_REJECT_PROJECT,
  TG_OTA_REJECT_CHIP,
  TG_OTA_REJECT_SIZE,
} tg_ota_decision;

tg_ota_decision tg_ota_request_check(const tg_ota_request *request);
unsigned tg_ota_progress_percent(size_t received, size_t total);

#endif
