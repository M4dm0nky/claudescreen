#ifndef TORGET_OTA_SERVICE_H
#define TORGET_OTA_SERVICE_H

#include <stdbool.h>

/*
 * Den autentiserade uppdateringstjänsten: en HTTP-lyssnare som strömmar
 * firmware till den INAKTIVA luckan, och bara när underhållsfönstret är
 * öppet. Alla accept/avslags-gränser bor i ota_policy (värdtestade);
 * här bor bara översättningen HTTP ↔ policy och esp_ota-livscykeln.
 *
 * Port 80 på LAN:et: token är grinden, inte transporten. Hotmodellen är
 * samma som resten av VibePulse — den som redan står i ditt WLAN.
 */
#define TG_OTA_HTTP_PORT 80

/* Starta lyssnaren. Idempotent; kallas direkt efter wifi_start() — även
 * utan IP binds sockeln och blir nåbar på det gränssnitt som kommer upp. */
void torget_ota_service_start(void);

/* KEY3-hållets väg in: öppna (eller förläng) underhållsfönstret tio
 * minuter. Endast atomärt tillstånd — säkert att kalla från LVGL-tasken. */
void torget_ota_service_open_maintenance(void);
/* Nödutgången: stänger fönstret i förtid. Atomär och säker att anropa från
 * LVGL-tasken — overlayn göms och servern stoppas av vakten, inte här. */
void torget_ota_service_close_maintenance(void);

/* OTA-annonsen fran kvotpollen (plattformens torget_update_available
 * vidarebefordrar hit): NULL/tom strang slacker annonsen. Jamforelsen mot
 * korande version och notisens tjatrytm bor i tjansten/notice_policy. */
void torget_ota_service_update_available(const char *version);

/* Är fönstret öppet just nu? Atomär läsning, säkert från alla taskar. */
bool torget_ota_service_maintenance_open(void);

#endif
