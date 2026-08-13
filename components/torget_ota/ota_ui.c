#include "ota_ui.h"

#include <stdio.h>

#include "lvgl.h"

#include "torget.h"

/*
 * Fyra stora statiska lägen, inget annat: inga kort, inga loggor, ingen
 * animation, inga opacitetslager, ingen appdata. Overlayn ska gå att läsa
 * tvärs över rummet medan en uppdatering pågår — och den ska vara omöjlig
 * att förväxla med en app.
 *
 * Fontval mot verklig glyftäckning (platform/fonts/fetch-and-convert.sh):
 * planens plex_text_32 rymmer bara "%GMWhkort" och plex_num_50 saknar
 * kolon, så lägesorden sätts i plex_attention_52 (versaler + mellanslag,
 * samma fullskärmsfont som NEEDS YOU-larmet) och nedräkningen i plex_ui_21
 * (full ASCII) — planens egen minsta tillåtna detaljfont. Procenten ryms i
 * plex_num_50 precis som planerat. Native storlekar, aldrig transformer.
 */

extern const lv_font_t plex_attention_52;
extern const lv_font_t plex_num_50;
extern const lv_font_t plex_ui_21;

#define COL_DETAIL lv_color_hex(0x8994A5) /* designsystemets etikettgrå  */
#define COL_TRACK  lv_color_hex(0x1D2634) /* designsystemets spårfärg    */

static struct {
  lv_obj_t *overlay;
  lv_obj_t *word;      /* UPDATES ON / RECEIVING / VERIFYING / RESTARTING */
  lv_obj_t *countdown; /* mm:ss kvar av fönstret (endast OPEN)            */
  lv_obj_t *percent;   /* "42%" (endast RECEIVING)                        */
  lv_obj_t *bar;       /* 20 px neutral balk (endast RECEIVING)           */
  tg_ota_ui_state rendered_state;
  unsigned rendered_percent;
  int rendered_seconds;
} ui;

void torget_ota_ui_create(void) {
  /* Topplagret, inte appträdet: apparnas rötter och drift-låda ligger under
   * lv_layer_top(), så overlayn kan aldrig knuffa en applayout. Kallas
   * under anroparens UI-lås (samma mönster som torget_ui_create). */
  ui.overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(ui.overlay);
  lv_obj_set_size(ui.overlay, 480, 480);
  lv_obj_set_pos(ui.overlay, 0, 0);
  lv_obj_set_style_bg_color(ui.overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(ui.overlay, LV_OPA_COVER, 0);
  /* Overlayn SLUKAR touch: annars når fingret apparna bakom svart glas. */
  lv_obj_add_flag(ui.overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(ui.overlay, LV_OBJ_FLAG_HIDDEN);

  ui.word = lv_label_create(ui.overlay);
  lv_obj_set_style_text_font(ui.word, &plex_attention_52, 0);
  lv_obj_set_style_text_color(ui.word, lv_color_white(), 0);
  lv_obj_align(ui.word, LV_ALIGN_CENTER, 0, -60);
  lv_label_set_text(ui.word, "");

  ui.countdown = lv_label_create(ui.overlay);
  lv_obj_set_style_text_font(ui.countdown, &plex_ui_21, 0);
  lv_obj_set_style_text_color(ui.countdown, COL_DETAIL, 0);
  lv_obj_set_style_text_letter_space(ui.countdown, 2, 0);
  lv_obj_align(ui.countdown, LV_ALIGN_CENTER, 0, 10);
  lv_label_set_text(ui.countdown, "");

  ui.percent = lv_label_create(ui.overlay);
  lv_obj_set_style_text_font(ui.percent, &plex_num_50, 0);
  lv_obj_set_style_text_color(ui.percent, lv_color_white(), 0);
  lv_obj_align(ui.percent, LV_ALIGN_CENTER, 0, 30);
  lv_label_set_text(ui.percent, "");

  /* Neutral balk: spårfärg ur designsystemet, grå fyllnad — ingen
   * provideraccent, det här är plattformens läge, inte en apps. */
  ui.bar = lv_bar_create(ui.overlay);
  lv_obj_remove_style_all(ui.bar);
  lv_obj_set_size(ui.bar, 360, 20);
  lv_obj_align(ui.bar, LV_ALIGN_CENTER, 0, 120);
  lv_obj_set_style_bg_color(ui.bar, COL_TRACK, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui.bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(ui.bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui.bar, COL_DETAIL, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(ui.bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(ui.bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_bar_set_range(ui.bar, 0, 100);

  ui.rendered_state = TG_OTA_UI_HIDDEN;
}

static const char *state_word(tg_ota_ui_state state) {
  switch (state) {
    case TG_OTA_UI_OPEN:       return "UPDATES ON";
    case TG_OTA_UI_RECEIVING:  return "RECEIVING";
    case TG_OTA_UI_VERIFYING:  return "VERIFYING";
    case TG_OTA_UI_RESTARTING: return "RESTARTING";
    default:                   return "";
  }
}

void torget_ota_ui_set(tg_ota_ui_state state, unsigned percent,
                       int seconds_left) {
  if (!ui.overlay) return;
  if (percent > 100) percent = 100;
  if (seconds_left < 0) seconds_left = 0;
  /* mm:ss-formatet bär max 99:59; fönstret är 10 min så taket är teori. */
  if (seconds_left > 99 * 60 + 59) seconds_left = 99 * 60 + 59;

  torget_ui_lock();
  /* Nyckla om-ritningen på synligt innehåll: underhållstasken pollar varje
   * halvsekund och ska inte invalidera pixlar som inte bytt värde. Nyckeln
   * läses under låset så två anropare aldrig ser en halvskriven trippel. */
  if (state == ui.rendered_state &&
      (state != TG_OTA_UI_RECEIVING || percent == ui.rendered_percent) &&
      (state != TG_OTA_UI_OPEN || seconds_left == ui.rendered_seconds)) {
    torget_ui_unlock();
    return;
  }
  ui.rendered_state = state;
  ui.rendered_percent = percent;
  ui.rendered_seconds = seconds_left;

  if (state == TG_OTA_UI_HIDDEN) {
    lv_obj_add_flag(ui.overlay, LV_OBJ_FLAG_HIDDEN);
    torget_ui_unlock();
    return;
  }

  lv_label_set_text(ui.word, state_word(state));

  if (state == TG_OTA_UI_OPEN) {
    char clock[8];
    snprintf(clock, sizeof clock, "%02d:%02d", seconds_left / 60,
             seconds_left % 60);
    lv_label_set_text(ui.countdown, clock);
    lv_obj_remove_flag(ui.countdown, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui.countdown, LV_OBJ_FLAG_HIDDEN);
  }

  if (state == TG_OTA_UI_RECEIVING) {
    char text[8];
    snprintf(text, sizeof text, "%u%%", percent);
    lv_label_set_text(ui.percent, text);
    lv_bar_set_value(ui.bar, (int32_t)percent, LV_ANIM_OFF);
    lv_obj_remove_flag(ui.percent, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ui.bar, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui.percent, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui.bar, LV_OBJ_FLAG_HIDDEN);
  }

  lv_obj_remove_flag(ui.overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui.overlay);
  torget_ui_unlock();
}
