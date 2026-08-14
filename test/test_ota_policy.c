#include <stdio.h>
#include <stddef.h>

#include "../components/torget_ota/ota_policy.h"

static int failures;

static void check(const char *what, int condition) {
  if (!condition) {
    printf("FAIL %s\n", what);
    failures++;
  }
}

/* En begäran som ska accepteras: rätt projekt, rätt chip, exakt på
 * 4 MiB-taket och med gott om plats i en 5 MiB-lucka. Varje test ändrar
 * sedan ett enda fält, så att avslagsorsaken alltid är entydig. */
static tg_ota_request valid_request(void) {
  tg_ota_request request = {
      .maintenance_open = true,
      .authorized = true,
      .project = "torget",
      .chip = "esp32s3",
      .content_length = TG_OTA_MAX_IMAGE_BYTES,
      .slot_size = 5U * 1024U * 1024U,
  };
  return request;
}

static void test_closed_window_rejects_everything(void) {
  tg_ota_request request = valid_request();
  request.maintenance_open = false;
  check("closed window rejects a fully valid upload",
        tg_ota_request_check(&request) == TG_OTA_REJECT_CLOSED);

  request.authorized = false;
  check("closed window outranks missing auth",
        tg_ota_request_check(&request) == TG_OTA_REJECT_CLOSED);

  check("null request counts as closed",
        tg_ota_request_check(NULL) == TG_OTA_REJECT_CLOSED);
}

static void test_auth_is_checked_before_metadata(void) {
  tg_ota_request request = valid_request();
  request.authorized = false;
  check("open window without auth is rejected",
        tg_ota_request_check(&request) == TG_OTA_REJECT_AUTH);

  /* Fel token och saknad token kollapsar till authorized=false vid
   * HTTP-gränsen; policyn får aldrig skvallra om projekt/chip/storlek
   * innan autentiseringen är avklarad. */
  request.project = "other-project";
  request.content_length = 0;
  check("unauthorized request leaks no metadata verdicts",
        tg_ota_request_check(&request) == TG_OTA_REJECT_AUTH);
}

static void test_project_and_chip_pinning(void) {
  tg_ota_request request = valid_request();
  request.project = "clawdmeter";
  check("wrong project is rejected",
        tg_ota_request_check(&request) == TG_OTA_REJECT_PROJECT);

  request.project = NULL;
  check("missing project is rejected",
        tg_ota_request_check(&request) == TG_OTA_REJECT_PROJECT);

  request = valid_request();
  request.chip = "esp32c3";
  check("wrong chip is rejected",
        tg_ota_request_check(&request) == TG_OTA_REJECT_CHIP);

  request.chip = NULL;
  check("missing chip is rejected",
        tg_ota_request_check(&request) == TG_OTA_REJECT_CHIP);

  request = valid_request();
  request.project = "Torget";
  check("project comparison is exact, not case folded",
        tg_ota_request_check(&request) == TG_OTA_REJECT_PROJECT);
}

static void test_size_boundaries(void) {
  tg_ota_request request = valid_request();
  request.content_length = 0;
  check("zero-length image is rejected",
        tg_ota_request_check(&request) == TG_OTA_REJECT_SIZE);

  request = valid_request();
  request.content_length = TG_OTA_MAX_IMAGE_BYTES + 1U;
  check("image one byte over the 4 MiB cap is rejected",
        tg_ota_request_check(&request) == TG_OTA_REJECT_SIZE);

  request = valid_request();
  request.content_length = 2U * 1024U * 1024U;
  request.slot_size = 1U * 1024U * 1024U;
  check("image over the slot size is rejected even under the cap",
        tg_ota_request_check(&request) == TG_OTA_REJECT_SIZE);

  request = valid_request();
  check("image exactly at the 4 MiB cap is accepted",
        tg_ota_request_check(&request) == TG_OTA_ACCEPT);

  request.content_length = 1U;
  check("small valid image is accepted",
        tg_ota_request_check(&request) == TG_OTA_ACCEPT);
}

static void test_progress_zero_total(void) {
  check("zero total reports zero percent",
        tg_ota_progress_percent(0, 0) == 0);
  check("received bytes with zero total still report zero",
        tg_ota_progress_percent(1234, 0) == 0);
}

static void test_progress_is_monotonic(void) {
  const size_t total = 4U * 1024U * 1024U;
  unsigned previous = 0;
  for (size_t received = 0; received <= total; received += 64U * 1024U) {
    unsigned percent = tg_ota_progress_percent(received, total);
    check("progress never runs backwards", percent >= previous);
    check("progress never exceeds 100", percent <= 100);
    previous = percent;
  }
  check("full receive reaches exactly 100", previous == 100);
}

static void test_progress_clamps_at_100(void) {
  check("received equal to total is 100",
        tg_ota_progress_percent(4096, 4096) == 100);
  check("received beyond total clamps to 100",
        tg_ota_progress_percent(5000, 4096) == 100);
  /* 100 % är ett löfte om att hela kroppen togs emot; en delvis
   * mottagen bild får aldrig avrundas upp till det löftet. */
  check("partial receive never reports 100",
        tg_ota_progress_percent(4095, 4096) < 100);
  check("halfway reports 50", tg_ota_progress_percent(2048, 4096) == 50);
}

int main(void) {
  test_closed_window_rejects_everything();
  test_auth_is_checked_before_metadata();
  test_project_and_chip_pinning();
  test_size_boundaries();
  test_progress_zero_total();
  test_progress_is_monotonic();
  test_progress_clamps_at_100();

  if (failures == 0) {
    printf("OK: all OTA request-policy tests pass\n");
    return 0;
  }
  printf("%d tests failed\n", failures);
  return 1;
}
