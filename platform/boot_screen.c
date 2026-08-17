#include "boot_screen.h"

#include "lvgl.h"

#include "torget.h"

/* Samma studio-tokens som OTA-ringen: vitt för det som hänt, muted för det
 * som väntar, äkta svart bakom. Wordmärket i attention-fonten (A-Z). */
extern const lv_font_t plex_attention_25;
extern const lv_font_t plex_ui_16;

#define COL_MUTED lv_color_hex(0x9298A2)

static struct {
  lv_obj_t *overlay;
  lv_obj_t *steps[3]; /* WIFI, TIME, DATA — muted tills signalen kommit */
  bool gone;          /* en nedtagen bootskärm kommer aldrig tillbaka   */
} ui;

static const char *const STEP_WORDS[3] = { "WIFI", "TIME", "DATA" };

void torget_boot_screen_create(void) {
  ui.overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(ui.overlay);
  lv_obj_set_size(ui.overlay, TORGET_SCREEN_W, TORGET_SCREEN_H);
  lv_obj_set_pos(ui.overlay, 0, 0);
  lv_obj_set_style_bg_color(ui.overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(ui.overlay, LV_OPA_COVER, 0);
  /* Slukar touch precis som OTA-overlayn: fingret ska inte nå halvbyggda
   * appvyer bakom ett svart lager. */
  lv_obj_add_flag(ui.overlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *wordmark = lv_label_create(ui.overlay);
  /* 52 px skulle mäta 284 px — bredare än glaset. 25:an mäter 135. */
  lv_obj_set_style_text_font(wordmark, &plex_attention_25, 0);
  lv_obj_set_style_text_color(wordmark, lv_color_white(), 0);
  lv_obj_align(wordmark, LV_ALIGN_TOP_MID, 0, 96);
  lv_label_set_text(wordmark, "VIBEPULSE");

  /* Tre steg med jämn luft, optiskt centrerade som grupp. Delningen följer
   * panelen: 72 px isär rymmer det bredaste ordet (48 px) utan att de rör
   * varandra, och gruppen håller sig innanför 240 px. */
  static const int STEP_X[3] = { -72, 0, 72 };
  for (int i = 0; i < 3; i++) {
    ui.steps[i] = lv_label_create(ui.overlay);
    lv_obj_set_style_text_font(ui.steps[i], &plex_ui_16, 0);
    lv_obj_set_style_text_color(ui.steps[i], COL_MUTED, 0);
    lv_obj_set_style_text_letter_space(ui.steps[i], 2, 0);
    lv_obj_align(ui.steps[i], LV_ALIGN_TOP_MID, STEP_X[i], 140);
    lv_label_set_text(ui.steps[i], STEP_WORDS[i]);
  }
}

void torget_boot_screen_stage(tg_boot_stage stage) {
  if (!ui.overlay || ui.gone) return;
  if (!torget_ui_try_lock(200)) return; /* nästa signal/poll försöker igen */
  switch (stage) {
    case TG_BOOT_WIFI_UP:
      lv_obj_set_style_text_color(ui.steps[0], lv_color_white(), 0);
      break;
    case TG_BOOT_TIME_OK:
      lv_obj_set_style_text_color(ui.steps[0], lv_color_white(), 0);
      lv_obj_set_style_text_color(ui.steps[1], lv_color_white(), 0);
      break;
    case TG_BOOT_DATA_OK:
    case TG_BOOT_GIVE_UP:
      /* Klart (eller uppgivet): bootskärmen är ett engångslager — riv den
       * så dess minne går tillbaka och den aldrig kan skymma något igen. */
      lv_obj_delete(ui.overlay);
      ui.overlay = NULL;
      ui.gone = true;
      break;
  }
  torget_ui_unlock();
}
