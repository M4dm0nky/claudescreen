#include "ota_ui.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"

#include "torget.h"

/*
 * Ringen (riktning A, vald 2026-08-14): ett fullsvart lager med lägesordet
 * överst, en cirkelbåge mitt på glaset och EN dominerande siffra i bågen.
 * Bågen är ärlig data i varje läge: kvarvarande lucktid när fönstret är
 * öppet, mottagen andel under RECEIVING, full cirkel när avbilden
 * verifieras och startas om. Inga kort, inga loggor, ingen appdata — och
 * ingen provideraccent: det här är plattformens läge, inte en apps.
 *
 * Statisk batch: all rörelse (andningspunkten på bågens huvud) väntar på
 * sin egen granskning EFTER den statiska fysiska granskningen — skillnaden
 * mellan godkänd stillbild och godkänd rörelse är två separata grindar.
 *
 * Fontval mot verklig glyftäckning (platform/fonts/fetch-and-convert.sh):
 * lägesorden i plex_attention_52 (versaler + mellanslag, samma
 * fullskärmsfont som NEEDS YOU-larmet), mittsiffran i plex_num_118 (siffror,
 * %, och sedan 2026-08-14 kolon — mm:ss-nedräkningen bor där), %-tecknet i
 * plex_num_50 och detaljraden i plex_ui_21 (full ASCII). Native storlekar,
 * aldrig transformer.
 */

extern const lv_font_t plex_attention_52;
extern const lv_font_t plex_num_118;
extern const lv_font_t plex_num_84;
extern const lv_font_t plex_num_50;
extern const lv_font_t plex_ui_21;

/* Färgerna är studio-tokens (design/vibepulse/studio-design.json): muted
 * och track därifrån, inte de äldre lokala nyanserna — rastret på glaset
 * ska gå att lägga bredvid studiomockupen utan att skilja sig. */
#define COL_DETAIL lv_color_hex(0x9298A2) /* palette.muted */
#define COL_TRACK  lv_color_hex(0x303238) /* palette.track */

/* Bågen: 296 px yttermått ger radie 148, spår 16 px — samma proportioner
 * som mockupen (2026-08-14, riktning A). Centrum (240, 268) lämnar plats
 * för lägesordet överst utan att sista raden når det klippta hörnglaset. */
#define ARC_SIZE   296
#define ARC_X      (240 - ARC_SIZE / 2)
#define ARC_Y      (268 - ARC_SIZE / 2)
#define ARC_WIDTH  16

/* Fönstret är tio minuter; bågen mappar sekunder → 0..100 mot det taket. */
#define WINDOW_SECONDS 600

static struct {
  lv_obj_t *overlay;
  lv_obj_t *word;    /* UPDATES ON / RECEIVING / VERIFYING / RESTARTING */
  lv_obj_t *arc;     /* lucktid (OPEN) eller mottagen andel (övriga)     */
  lv_obj_t *center;  /* mm:ss (OPEN) eller procentsiffrorna (övriga)    */
  lv_obj_t *pctsign; /* "%" bredvid siffrorna — dolt i OPEN             */
  lv_obj_t *detail;  /* KEY3 CLOSES / SHA-256 — tomt annars             */
  lv_obj_t *version; /* körande version (OPEN) / inkommande (övriga)    */
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
  lv_obj_align(ui.word, LV_ALIGN_TOP_MID, 0, 52);
  lv_label_set_text(ui.word, "");

  /* Bågen är ren visning: ingen knopp, inget klick — värdet sätts bara
   * härifrån. Startar kl 12 och fylls medurs (rotation 270). */
  ui.arc = lv_arc_create(ui.overlay);
  lv_obj_remove_style_all(ui.arc);
  lv_obj_set_size(ui.arc, ARC_SIZE, ARC_SIZE);
  lv_obj_set_pos(ui.arc, ARC_X, ARC_Y);
  lv_arc_set_rotation(ui.arc, 270);
  lv_arc_set_bg_angles(ui.arc, 0, 360);
  lv_arc_set_range(ui.arc, 0, 100);
  lv_obj_set_style_arc_color(ui.arc, COL_TRACK, LV_PART_MAIN);
  lv_obj_set_style_arc_width(ui.arc, ARC_WIDTH, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(ui.arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_color(ui.arc, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(ui.arc, ARC_WIDTH, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(ui.arc, true, LV_PART_INDICATOR);
  lv_obj_remove_flag(ui.arc, LV_OBJ_FLAG_CLICKABLE);
  lv_arc_set_value(ui.arc, 0);

  ui.center = lv_label_create(ui.overlay);
  lv_obj_set_style_text_font(ui.center, &plex_num_118, 0);
  lv_obj_set_style_text_color(ui.center, lv_color_white(), 0);
  lv_label_set_text(ui.center, "");

  ui.pctsign = lv_label_create(ui.overlay);
  lv_obj_set_style_text_font(ui.pctsign, &plex_num_50, 0);
  lv_obj_set_style_text_color(ui.pctsign, COL_DETAIL, 0);
  lv_label_set_text(ui.pctsign, "%");
  lv_obj_add_flag(ui.pctsign, LV_OBJ_FLAG_HIDDEN);

  ui.detail = lv_label_create(ui.overlay);
  lv_obj_set_style_text_font(ui.detail, &plex_ui_21, 0);
  lv_obj_set_style_text_color(ui.detail, COL_DETAIL, 0);
  lv_obj_set_style_text_letter_space(ui.detail, 2, 0);
  lv_label_set_text(ui.detail, "");

  /* Versionsraden under ringen, centrerad — mitt-botten är synligt glas
   * även med de klippta hörnen (hörnlärdomen från brödsmulorna). */
  ui.version = lv_label_create(ui.overlay);
  lv_obj_set_style_text_font(ui.version, &plex_ui_21, 0);
  lv_obj_set_style_text_color(ui.version, COL_DETAIL, 0);
  lv_obj_set_style_text_letter_space(ui.version, 2, 0);
  lv_obj_align(ui.version, LV_ALIGN_TOP_MID, 0, 268 + ARC_SIZE / 2 + 10);
  lv_label_set_text(ui.version, "");

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

/* Detaljraden säger bara det ärliga: utgången (kort tryck, inte håll) när
 * fönstret väntar, och vad verifieringen faktiskt räknar. Övriga lägen bär
 * sitt ord och sin siffra — en dominerande siffra, inga bihang. */
static const char *state_detail(tg_ota_ui_state state) {
  switch (state) {
    case TG_OTA_UI_OPEN:      return "KEY3 CLOSES";
    case TG_OTA_UI_VERIFYING: return "SHA-256";
    default:                  return "";
  }
}

void torget_ota_ui_set(tg_ota_ui_state state, unsigned percent,
                       int seconds_left) {
  if (!ui.overlay) return;
  if (percent > 100) percent = 100;
  if (seconds_left < 0) seconds_left = 0;
  /* mm:ss-formatet bär max 99:59; fönstret är 10 min så taket är teori. */
  if (seconds_left > 99 * 60 + 59) seconds_left = 99 * 60 + 59;

  /* Tidsbegränsat lås: får vi inte UI-låset på 200 ms hoppar vi över den
   * här bildrutan i stället för att blockera — nästa halvsekundpoll gör om
   * försöket. En overlay som släpar en sekund är kosmetik; en OTA-task som
   * står fast i ett evigt låsförsök var exakt så panelen frös 2026-08-14. */
  if (!torget_ui_try_lock(200)) return;
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
  lv_label_set_text(ui.detail, state_detail(state));

  if (state == TG_OTA_UI_OPEN) {
    /* Bågen är kvarvarande lucktid: full vid nytt håll, dränerad vid 0.
     * REVERSE ankrar den vita bågen i slutvinkeln (toppen) och ritar
     * värdets andel BAKÅT — startkanten vandrar då medurs från toppen när
     * tiden rinner, som en äggklocka. NORMAL-läget lät slutkanten krypa
     * moturs, vilket kändes baklänges på glaset (fysisk granskning
     * 2026-08-14). Klockan sätts i 84:an — 118:an svämmade över ringens
     * innerradie (rastergranskningen samma dag). */
    lv_arc_set_mode(ui.arc, LV_ARC_MODE_REVERSE);
    lv_arc_set_value(ui.arc,
                     (int32_t)((seconds_left * 100) / WINDOW_SECONDS));
    char clock[8];
    snprintf(clock, sizeof clock, "%02d:%02d", seconds_left / 60,
             seconds_left % 60);
    lv_obj_set_style_text_font(ui.center, &plex_num_84, 0);
    lv_label_set_text(ui.center, clock);
    lv_obj_add_flag(ui.pctsign, LV_OBJ_FLAG_HIDDEN);
  } else {
    /* RECEIVING fyller bågen medurs med mottagen andel; VERIFYING/
     * RESTARTING anropas med 100 och sluter cirkeln. */
    lv_arc_set_mode(ui.arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_value(ui.arc, (int32_t)percent);
    char digits[8];
    snprintf(digits, sizeof digits, "%u", percent);
    lv_obj_set_style_text_font(ui.center, &plex_num_118, 0);
    lv_label_set_text(ui.center, digits);
    lv_obj_remove_flag(ui.pctsign, LV_OBJ_FLAG_HIDDEN);
  }

  /* Centrera mot bågens mitt när innehållet bytt bredd; %-tecknet hänger
   * på siffrornas nederkant som i mockupen. Detaljraden ligger under
   * mittsiffran, väl innanför bågens innerradie. */
  lv_obj_align(ui.center, LV_ALIGN_CENTER, 0, 268 - 240);
  lv_obj_align_to(ui.pctsign, ui.center, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -14);
  lv_obj_align(ui.detail, LV_ALIGN_CENTER, 0, 268 - 240 + 78);

  lv_obj_remove_flag(ui.overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui.overlay);
  torget_ui_unlock();
}

void torget_ota_ui_set_version(const char *version) {
  if (!ui.overlay || !version) return;
  if (!torget_ui_try_lock(200)) return;
  /* Dedupe under låset: vakten återpushar körande version varje halvsekund
   * (så en avbruten uppladdnings inkommande rad självläker tillbaka) och
   * oförändrad text får inte bli en invalidering per poll. */
  if (strcmp(lv_label_get_text(ui.version), version) != 0)
    lv_label_set_text(ui.version, version);
  torget_ui_unlock();
}
