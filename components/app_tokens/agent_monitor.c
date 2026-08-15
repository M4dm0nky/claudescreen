#include "agent_monitor.h"

#include <stdio.h>
#include <string.h>

#include "agent_assets.h"
#include "agent_completion_policy.h"
#include "agent_monitor_policy.h"
#include "needs_you_policy.h"
#include "torget.h"
#include "vibepulse_layout.generated.h"

extern const lv_font_t plex_attention_18;
extern const lv_font_t plex_attention_25;
extern const lv_font_t plex_attention_52;
extern const lv_font_t plex_ui_14;
/* The only rasters that carry lowercase + punctuation are the plex_ui_* family
 * (0x20-0x7E); arbitrary Claude text — a question, a command — can only be
 * drawn with these, so 21 px is the largest a title/command can be. Both are
 * already linked (usage_screen, boot, ota, star popup), so this adds no flash. */
extern const lv_font_t plex_ui_16;
extern const lv_font_t plex_ui_21;

#define COL_BLACK   lv_color_hex(VP_COLOR_BACKGROUND)
#define COL_WHITE   lv_color_hex(VP_COLOR_TEXT)
#define COL_CLAUDE  lv_color_hex(VP_COLOR_CLAUDE)
#define COL_CODEX   lv_color_hex(VP_COLOR_CODEX)
#define COL_MUTED   lv_color_hex(VP_COLOR_MUTED)
#define COL_TRACK   lv_color_hex(VP_COLOR_TRACK)
/* A faint card lift so the recommendation groups without breaking the true
 * black ground; a surface tone, never a data colour. */
#define NY_CARD_BG   lv_color_hex(0x14161C)
#define NY_CARD_LINE lv_color_hex(0x2A2F38)

typedef struct {
  lv_obj_t *root;
  lv_obj_t *outline;
  lv_obj_t *provider;
  lv_obj_t *icon_ring;
  lv_obj_t *claude_icon;
  lv_obj_t *codex_icon;
  lv_obj_t *title;
  lv_obj_t *project;
  lv_obj_t *detail;
  lv_obj_t *dismiss;
} completion_view;

/* The interactive "Needs You" takeover. One object tree, built hidden, updated
 * from needs_you_policy on every poll. */
typedef struct {
  lv_obj_t *root;
  lv_obj_t *outline;
  lv_obj_t *eyebrow;
  lv_obj_t *project;
  lv_obj_t *prompt;
  lv_obj_t *card;
  lv_obj_t *card_eyebrow;
  lv_obj_t *card_title;
  lv_obj_t *card_subtitle;
  lv_obj_t *private_title;
  lv_obj_t *private_hint;
  lv_obj_t *approve;
  lv_obj_t *deny;
  lv_obj_t *leave;
  lv_obj_t *more_options;
  lv_obj_t *desk_hint;
  lv_obj_t *countdown;
} needs_you_view;

/* Enough of the decision to know when a repaint is actually needed; the
 * countdown only moves on a poll, so ticks in between are free. */
typedef struct {
  bool valid;
  bool visible;
  bool offer_approve;
  bool offer_deny;
  bool marked;
  bool has_title;
  uint8_t kind;
  uint8_t options_total;
  uint32_t seconds_left;
  char request_id[TK_PENDING_ID_CAP];
} needs_you_key;

static struct {
  completion_view completion;
  needs_you_view needs_you;
  tk_needs_you_state needs_you_state;
  needs_you_key needs_you_rendered;
  bool needs_you_visible; /* read by render_completion to yield the screen */
  tk_agent_monitor_needs_you_cb tk_needs_you_cb;
  tk_agent_snapshot snapshot;
  tk_completion_queue queue;
  tk_completion_render_key rendered_completion;
  int64_t applied_at_us;
  int64_t rendered_at_us;
  bool has_snapshot;
  bool suppress_click;
} mon;

static lv_obj_t *bare(lv_obj_t *parent) {
  lv_obj_t *object = lv_obj_create(parent);
  lv_obj_remove_style_all(object);
  lv_obj_set_size(object, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
  return object;
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font,
                       lv_color_t color) {
  lv_obj_t *object = lv_label_create(parent);
  lv_obj_set_style_text_font(object, font, 0);
  lv_obj_set_style_text_color(object, color, 0);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
  return object;
}

static lv_obj_t *create_codex_icon(lv_obj_t *parent, int x, int y) {
  lv_obj_t *image = lv_image_create(parent);
  lv_image_set_src(image, &tk_img_codex);
  lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(image, x, y);
  return image;
}

/* Pulsen: accentkonturen och ikonringen andas i opacitet under larmets
 * PULSE-fas. Rörelse i befintliga element, aldrig nya ytor; texten står
 * still för läsbarhet. Cyklerna à 1200 ms fyller TK_COMPLETION_PULSE_MS
 * (45 s sedan 2026-08-14 — 4,8 s missades i praktiken), sedan vilar allt
 * på full opacitet i STATIC. */
#define COMPLETION_PULSE_CYCLE_MS 1200U
#define COMPLETION_PULSE_MIN_OPA 100

static void completion_pulse_exec(void *var, int32_t value) {
  (void)var;
  lv_obj_set_style_border_opa(mon.completion.outline, (lv_opa_t)value, 0);
  lv_obj_set_style_border_opa(mon.completion.icon_ring, (lv_opa_t)value, 0);
}

static void completion_pulse_stop(void) {
  lv_anim_delete(&mon.completion, completion_pulse_exec);
  lv_obj_set_style_border_opa(mon.completion.outline, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(mon.completion.icon_ring, LV_OPA_COVER, 0);
}

static void completion_pulse_start(void) {
  completion_pulse_stop();
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, &mon.completion);
  lv_anim_set_exec_cb(&anim, completion_pulse_exec);
  lv_anim_set_values(&anim, LV_OPA_COVER, COMPLETION_PULSE_MIN_OPA);
  lv_anim_set_duration(&anim, COMPLETION_PULSE_CYCLE_MS / 2);
  lv_anim_set_playback_duration(&anim, COMPLETION_PULSE_CYCLE_MS / 2);
  lv_anim_set_repeat_count(&anim,
                           TK_COMPLETION_PULSE_MS / COMPLETION_PULSE_CYCLE_MS);
  lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
  lv_anim_start(&anim);
}

static void render_completion(uint64_t now_ms) {
  tk_completion_phase phase = tk_completion_phase_at(&mon.queue, now_ms);
  const tk_completion_event *event =
      tk_completion_queue_current(&mon.queue);
  /* The interactive takeover owns the screen while it is up: a live blocked
   * session outranks the passive DONE/NEEDS-YOU pulse, and folding it into the
   * render key means the pulse cleanly reappears the moment the takeover
   * leaves. */
  bool visible = event && phase != TK_COMPLETION_HIDDEN && !mon.needs_you_visible;
  if (!tk_completion_render_key_update(&mon.rendered_completion, event,
                                       visible)) return;
  if (!visible) {
    completion_pulse_stop();
    lv_obj_add_flag(mon.completion.root, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  int provider = event->provider;
  lv_color_t accent = provider == TK_AGENT_PROVIDER_CLAUDE
                          ? COL_CLAUDE : COL_CODEX;
  const char *provider_name = provider == TK_AGENT_PROVIDER_CLAUDE
                                  ? "CLAUDE" : "CODEX";
  lv_obj_remove_flag(mon.completion.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(mon.completion.root);
  lv_label_set_text(mon.completion.provider, provider_name);
  lv_obj_set_style_text_color(mon.completion.provider, accent, 0);
  lv_obj_set_style_border_color(mon.completion.outline, accent, 0);
  lv_obj_set_style_border_color(mon.completion.icon_ring, accent, 0);
  if (provider == TK_AGENT_PROVIDER_CLAUDE) {
    lv_obj_remove_flag(mon.completion.claude_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mon.completion.codex_icon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(mon.completion.claude_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(mon.completion.codex_icon, LV_OBJ_FLAG_HIDDEN);
  }

  char project[TK_AGENT_PROJECT_CAP];
  tk_agent_monitor_project_label(event->project, project, sizeof project);
  lv_label_set_text(mon.completion.project, project);
  lv_obj_set_style_text_color(mon.completion.project, accent, 0);
  if (project[0]) lv_obj_remove_flag(mon.completion.project,
                                     LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(mon.completion.project, LV_OBJ_FLAG_HIDDEN);

  char detail[32];
  const char *title = "DONE";
  if (event->state == TK_AGENT_WAITING) {
    title = "NEEDS YOU";
    if (event->same_state_count > 1) {
      snprintf(detail, sizeof detail, "%u AGENTS WAITING",
               (unsigned)event->same_state_count);
    } else {
      snprintf(detail, sizeof detail, "%s",
               provider == TK_AGENT_PROVIDER_CLAUDE
                   ? "CLAUDE IS WAITING" : "CODEX IS WAITING");
    }
  } else if (event->state == TK_AGENT_ERROR) {
    title = "ERROR";
    if (event->same_state_count > 1) {
      snprintf(detail, sizeof detail, "%u AGENTS NEED ATTENTION",
               (unsigned)event->same_state_count);
    } else {
      snprintf(detail, sizeof detail, "%s",
               provider == TK_AGENT_PROVIDER_CLAUDE
                   ? "CLAUDE NEEDS ATTENTION" : "CODEX NEEDS ATTENTION");
    }
  } else if (event->same_state_count > 1) {
    snprintf(detail, sizeof detail, "%u AGENTS FINISHED",
             (unsigned)event->same_state_count);
  } else {
    snprintf(detail, sizeof detail, "%s",
             provider == TK_AGENT_PROVIDER_CLAUDE
                 ? "CLAUDE FINISHED" : "CODEX FINISHED");
  }
  lv_label_set_text(mon.completion.title, title);
  lv_label_set_text(mon.completion.detail, detail);
  /* Ny alert eller nytt tillstånd på skärmen = ny puls. Startar även när
   * antalet i samma tillstånd växer — ny information förtjänar en andning. */
  completion_pulse_start();
}

static void completion_event(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_LONG_PRESSED) {
    mon.suppress_click = true;
    torget_launcher_open();
    return;
  }
  if (code != LV_EVENT_CLICKED) return;
  if (mon.suppress_click) {
    mon.suppress_click = false;
    return;
  }
  tk_completion_queue_dismiss(&mon.queue);
  render_completion(mon.queue.last_now_ms);
}

static void create_completion(lv_obj_t *app_root) {
  completion_view *view = &mon.completion;
  view->root = bare(app_root);
  lv_obj_set_pos(view->root, 0, 0);
  lv_obj_set_size(view->root, VP_SCREEN_W, VP_SCREEN_H);
  lv_obj_set_style_bg_opa(view->root, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(view->root, COL_BLACK, 0);
  lv_obj_add_flag(view->root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(view->root, completion_event, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(view->root, completion_event,
                      LV_EVENT_LONG_PRESSED, NULL);

  view->outline = bare(view->root);
  lv_obj_set_pos(view->outline, 8, 8);
  lv_obj_set_size(view->outline, 464, 464);
  lv_obj_set_style_bg_opa(view->outline, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(view->outline, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(view->outline, 6, 0);
  lv_obj_set_style_radius(view->outline, 36, 0);

  view->provider = label(view->root, &plex_attention_18, COL_WHITE);
  lv_obj_set_pos(view->provider, 20, 31);
  lv_obj_set_size(view->provider, 440, 25);
  lv_obj_set_style_text_align(view->provider, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(view->provider, 3, 0);

  view->icon_ring = bare(view->root);
  lv_obj_set_pos(view->icon_ring, 172, 77);
  lv_obj_set_size(view->icon_ring, 136, 136);
  lv_obj_set_style_bg_opa(view->icon_ring, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(view->icon_ring, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(view->icon_ring, 3, 0);
  lv_obj_set_style_radius(view->icon_ring, LV_RADIUS_CIRCLE, 0);

  lv_obj_t *claude_group = bare(view->root);
  lv_obj_set_pos(claude_group, 184, 89);
  lv_obj_set_size(claude_group, 112, 112);
  view->claude_icon = lv_image_create(claude_group);
  lv_image_set_src(view->claude_icon, &tk_img_claude);
  lv_obj_set_size(view->claude_icon, 112, 112);
  lv_image_set_inner_align(view->claude_icon, LV_IMAGE_ALIGN_STRETCH);
  lv_obj_set_pos(view->claude_icon, 0, 0);
  lv_obj_set_style_image_recolor(view->claude_icon, COL_CLAUDE, 0);
  lv_obj_set_style_image_recolor_opa(view->claude_icon, LV_OPA_COVER, 0);

  view->codex_icon = create_codex_icon(view->root, 184, 89);

  view->title = label(view->root, &plex_attention_52, COL_WHITE);
  lv_obj_set_pos(view->title, 14, 246);
  lv_obj_set_size(view->title, 452, 68);
  lv_obj_set_style_text_align(view->title, LV_TEXT_ALIGN_CENTER, 0);

  view->project = label(view->root, &plex_attention_25, COL_WHITE);
  lv_obj_set_pos(view->project, 20, 321);
  lv_obj_set_size(view->project, 440, 34);
  lv_obj_set_style_text_align(view->project, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(view->project, 2, 0);

  view->detail = label(view->root, &plex_ui_14, COL_MUTED);
  lv_obj_set_pos(view->detail, 20, 365);
  lv_obj_set_size(view->detail, 440, 25);
  lv_obj_set_style_text_align(view->detail, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(view->detail, 2, 0);

  view->dismiss = label(view->root, &plex_ui_14, COL_MUTED);
  lv_obj_set_pos(view->dismiss, 20, 430);
  lv_obj_set_size(view->dismiss, 440, 26);
  lv_obj_set_style_text_align(view->dismiss, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_letter_space(view->dismiss, 2, 0);
  lv_label_set_text(view->dismiss, "TAP TO DISMISS");

  lv_obj_add_flag(view->root, LV_OBJ_FLAG_HIDDEN);
}

/* ---------------------------------------------------------------- Needs You */

static void render_needs_you(void);

static void needs_you_verdict_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  tk_needs_you_verdict verdict =
      (tk_needs_you_verdict)(intptr_t)lv_event_get_user_data(event);
  const tk_pending_interaction *pending = &mon.snapshot.pending;
  tk_needs_you_view decision =
      tk_needs_you_view_of(&mon.needs_you_state, pending);
  /* The frame's own gate. A stray touch, a stale frame or a mis-wired callback
   * dies here rather than sending an APPROVE the policy never offered. */
  if (!tk_needs_you_allows(pending, &decision, verdict)) return;
  if (mon.tk_needs_you_cb) mon.tk_needs_you_cb(verdict, pending->request_id);
  /* Drop the takeover at the tap instead of a poll later, so a second tap can
   * not land on the same prompt. */
  tk_needs_you_mark_answered(&mon.needs_you_state, pending);
  render_needs_you();
}

static void ny_show(lv_obj_t *object, bool shown) {
  if (shown) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

/* Tool names are ASCII letters; the label font is uppercase-only, so fold the
 * one dynamic string that shares it with "CLAUDE RECOMMENDS". */
static void ny_ascii_upper(const char *source, char *destination, size_t cap) {
  size_t i = 0;
  if (!destination || cap == 0) return;
  for (; source && source[i] && i + 1 < cap; i++) {
    char c = source[i];
    destination[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
  }
  destination[i] = '\0';
}

static lv_obj_t *ny_label(lv_obj_t *parent, const lv_font_t *font,
                          lv_color_t color, int x, int y, int w,
                          int32_t letter_space) {
  lv_obj_t *object = label(parent, font, color);
  lv_obj_set_pos(object, x, y);
  lv_obj_set_size(object, w, LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
  if (letter_space) lv_obj_set_style_text_letter_space(object, letter_space, 0);
  return object;
}

static lv_obj_t *ny_button(lv_obj_t *parent, int x, int y, int w, int h,
                           const lv_font_t *font, lv_color_t text_color,
                           lv_color_t border_color, const char *text,
                           tk_needs_you_verdict verdict) {
  lv_obj_t *btn = bare(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(btn, border_color, 0);
  lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_radius(btn, 14, 0);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btn, needs_you_verdict_event, LV_EVENT_CLICKED,
                      (void *)(intptr_t)verdict);
  lv_obj_t *lbl = label(btn, font, text_color);
  lv_label_set_text(lbl, text);
  lv_obj_center(lbl);
  return btn;
}

static void create_needs_you(lv_obj_t *app_root) {
  needs_you_view *v = &mon.needs_you;
  v->root = bare(app_root);
  lv_obj_set_pos(v->root, 0, 0);
  lv_obj_set_size(v->root, VP_SCREEN_W, VP_SCREEN_H);
  lv_obj_set_style_bg_opa(v->root, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(v->root, COL_BLACK, 0);
  /* Deliberately not clickable: only the explicit controls resolve a decision,
   * so a stray tap on the ground does nothing. This is not a dismiss-anywhere
   * alert — the terminal is the fallback, never a blank tap. */

  v->outline = bare(v->root);
  lv_obj_set_pos(v->outline, 8, 8);
  lv_obj_set_size(v->outline, 464, 464);
  lv_obj_set_style_bg_opa(v->outline, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(v->outline, COL_CLAUDE, 0);
  lv_obj_set_style_border_opa(v->outline, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(v->outline, 6, 0);
  lv_obj_set_style_radius(v->outline, 36, 0);

  v->eyebrow = ny_label(v->root, &plex_attention_18, COL_CLAUDE, 24, 28, 432, 3);
  v->project = ny_label(v->root, &plex_attention_25, COL_WHITE, 24, 52, 432, 2);
  v->prompt = ny_label(v->root, &plex_ui_16, COL_MUTED, 28, 90, 424, 0);
  lv_label_set_long_mode(v->prompt, LV_LABEL_LONG_WRAP);

  v->card = bare(v->root);
  lv_obj_set_pos(v->card, 24, 132);
  lv_obj_set_size(v->card, 432, 120);
  lv_obj_set_style_bg_color(v->card, NY_CARD_BG, 0);
  lv_obj_set_style_bg_opa(v->card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(v->card, NY_CARD_LINE, 0);
  lv_obj_set_style_border_opa(v->card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(v->card, 1, 0);
  lv_obj_set_style_radius(v->card, 20, 0);

  v->card_eyebrow =
      ny_label(v->card, &plex_attention_18, COL_CLAUDE, 10, 14, 412, 2);
  v->card_title = ny_label(v->card, &plex_ui_21, COL_WHITE, 10, 40, 412, 0);
  lv_label_set_long_mode(v->card_title, LV_LABEL_LONG_WRAP);
  v->card_subtitle = ny_label(v->card, &plex_ui_16, COL_MUTED, 10, 88, 412, 0);

  v->private_title =
      ny_label(v->root, &plex_attention_25, COL_WHITE, 28, 150, 424, 1);
  v->private_hint = ny_label(v->root, &plex_ui_16, COL_MUTED, 28, 192, 424, 0);

  v->approve = ny_button(v->root, 24, 262, 432, 58, &plex_attention_25,
                         COL_CLAUDE, COL_CLAUDE, "APPROVE",
                         TK_NEEDS_YOU_VERDICT_APPROVE);
  v->deny = ny_button(v->root, 24, 330, 204, 50, &plex_attention_18, COL_WHITE,
                      COL_TRACK, "DENY", TK_NEEDS_YOU_VERDICT_DENY);
  v->leave = ny_button(v->root, 24, 330, 432, 50, &plex_attention_18, COL_MUTED,
                       COL_TRACK, "LEAVE IT", TK_NEEDS_YOU_VERDICT_LEAVE_IT);

  v->more_options = ny_label(v->root, &plex_ui_14, COL_MUTED, 24, 388, 432, 1);
  lv_label_set_text(v->more_options, "MORE OPTIONS IN TERMINAL");
  v->desk_hint = ny_label(v->root, &plex_attention_18, COL_MUTED, 24, 388, 432, 2);
  lv_label_set_text(v->desk_hint, "APPROVE AT YOUR DESK");
  v->countdown = ny_label(v->root, &plex_ui_14, COL_MUTED, 24, 424, 432, 1);

  lv_obj_add_flag(v->root, LV_OBJ_FLAG_HIDDEN);
}

/* Paint what the policy allows. Reads tk_needs_you_view_of / _allows; decides
 * nothing itself. Cheap to call every tick: it repaints only when the decision
 * that matters actually changes. */
static void render_needs_you(void) {
  needs_you_view *v = &mon.needs_you;
  const tk_pending_interaction *p = &mon.snapshot.pending;
  tk_needs_you_view decision = tk_needs_you_view_of(&mon.needs_you_state, p);
  mon.needs_you_visible = decision.visible;

  needs_you_key key;
  memset(&key, 0, sizeof key); /* padding too, so memcmp below is exact */
  key.valid = true;
  key.visible = decision.visible;
  key.offer_approve = decision.offer_approve;
  key.offer_deny = decision.offer_deny;
  key.marked = p->marked;
  key.has_title = p->has_title;
  key.kind = (uint8_t)decision.kind;
  key.options_total = p->options_total;
  key.seconds_left = decision.seconds_left;
  if (decision.visible)
    memcpy(key.request_id, p->request_id, sizeof key.request_id);
  if (mon.needs_you_rendered.valid &&
      memcmp(&mon.needs_you_rendered, &key, sizeof key) == 0)
    return;
  memcpy(&mon.needs_you_rendered, &key, sizeof key);

  if (!decision.visible) {
    ny_show(v->root, false);
    return;
  }

  bool is_question = decision.kind == TK_PENDING_QUESTION;
  bool offer_approve = decision.offer_approve;
  /* A question is answered or left to the terminal; DENY is a permission verb,
   * so it only rides the approval flow. */
  bool offer_deny = decision.offer_deny && !is_question;
  bool show_card = p->has_title;

  lv_label_set_text(v->eyebrow,
                    is_question ? "CLAUDE NEEDS YOU" : "APPROVAL REQUIRED");

  char project[TK_AGENT_PROJECT_CAP];
  tk_agent_monitor_project_label(p->has_project ? p->project : "", project,
                                 sizeof project);
  lv_label_set_text(v->project, project);
  ny_show(v->project, project[0] != '\0');

  ny_show(v->prompt, is_question && p->has_prompt);
  if (is_question && p->has_prompt) lv_label_set_text(v->prompt, p->prompt);

  ny_show(v->card, show_card);
  ny_show(v->private_title, !show_card);
  ny_show(v->private_hint, !show_card);
  if (show_card) {
    if (is_question) {
      ny_show(v->card_eyebrow, p->marked);
      if (p->marked) lv_label_set_text(v->card_eyebrow, "CLAUDE RECOMMENDS");
    } else {
      ny_show(v->card_eyebrow, p->has_tool);
      if (p->has_tool) {
        char tool[TK_PENDING_TOOL_CAP];
        ny_ascii_upper(p->tool, tool, sizeof tool);
        lv_label_set_text(v->card_eyebrow, tool);
      }
    }
    lv_label_set_text(v->card_title, p->has_title ? p->title : "");
    ny_show(v->card_subtitle, p->has_subtitle);
    if (p->has_subtitle) lv_label_set_text(v->card_subtitle, p->subtitle);
  } else {
    lv_label_set_text(v->private_title, "SOMETHING IS WAITING");
    lv_label_set_text(v->private_hint, "DETAILS STAY ON THE MAC");
  }

  ny_show(v->approve, offer_approve);
  ny_show(v->deny, offer_deny);
  ny_show(v->leave, true);
  /* LEAVE IT fills the row alone for a question, shares it with DENY when the
   * approval flow surfaces both. */
  if (offer_deny) {
    lv_obj_set_pos(v->leave, 252, 330);
    lv_obj_set_size(v->leave, 204, 50);
  } else {
    lv_obj_set_pos(v->leave, 24, 330);
    lv_obj_set_size(v->leave, 432, 50);
  }

  ny_show(v->more_options,
          is_question && offer_approve && p->options_total > 1);
  ny_show(v->desk_hint, !offer_approve);

  char countdown[48];
  snprintf(countdown, sizeof countdown,
           "%us left \xC2\xB7 then the terminal asks",
           (unsigned)decision.seconds_left);
  lv_label_set_text(v->countdown, countdown);

  ny_show(v->root, true);
  lv_obj_move_foreground(v->root);
}

void tk_agent_monitor_set_needs_you_cb(tk_agent_monitor_needs_you_cb cb) {
  mon.tk_needs_you_cb = cb;
}

void tk_agent_monitor_create(lv_obj_t *app_root) {
  memset(&mon, 0, sizeof mon);
  create_completion(app_root);
  create_needs_you(app_root);
}

void tk_agent_monitor_apply(const tk_agent_snapshot *snapshot,
                            int64_t now_us) {
  if (!snapshot) return;
  mon.snapshot = *snapshot;
  mon.applied_at_us = now_us;
  mon.rendered_at_us = now_us;
  mon.has_snapshot = true;
  tk_completion_queue_apply(&mon.queue, snapshot,
                            now_us > 0 ? (uint64_t)now_us / 1000ULL : 0);
  render_needs_you(); /* first: it decides whether completion yields the glass */
  render_completion(now_us > 0 ? (uint64_t)now_us / 1000ULL : 0);

  const tk_agent_provider_status *providers[2] = {
      &snapshot->claude, &snapshot->codex,
  };
  for (int provider = 0; provider < 2; provider++) {
    for (uint8_t i = 0; i < providers[provider]->job_count; i++) {
      if (tk_agent_monitor_should_keep_awake(&providers[provider]->jobs[i],
                                             "", 0)) {
        torget_keep_awake();
        return;
      }
    }
  }
}

void tk_agent_monitor_tick(int64_t now_us) {
  if (!mon.has_snapshot) return;
  mon.rendered_at_us = now_us;
  render_needs_you();
  render_completion(now_us > 0 ? (uint64_t)now_us / 1000ULL : 0);
}

void tk_agent_monitor_dismiss_current(void) {
  tk_completion_queue_dismiss(&mon.queue);
  render_completion(mon.queue.last_now_ms);
}
