#include "project_star_chime.h"

#include <stddef.h>

#include "app_tokens_config.h"

/* A5 -> C#6: a warm rising major third. At 258 ms total and 12% requested
 * level it is meant to read as a tiny physical acknowledgement, not an
 * alarm. The future codec backend owns the exact gain calibration. */
static const tk_project_star_chime_sequence SEQUENCE = {
    .notes = {
        {.frequency_hz = 880,
         .duration_ms = 78,
         .gap_after_ms = 18,
         .attack_ms = 8,
         .release_ms = 20,
         .level_percent = 12},
        {.frequency_hz = 1109,
         .duration_ms = 162,
         .gap_after_ms = 0,
         .attack_ms = 10,
         .release_ms = 38,
         .level_percent = 12},
    },
};

static tk_project_star_chime_backend registered_backend;
static void *registered_context;

void tk_project_star_chime_set_backend(tk_project_star_chime_backend backend,
                                       void *context) {
  registered_backend = backend;
  registered_context = backend ? context : NULL;
}

const tk_project_star_chime_sequence *tk_project_star_chime_get_sequence(void) {
  return &SEQUENCE;
}

bool tk_project_star_chime_request(void) {
#if TK_GITHUB_SOUND_ENABLED
  if (registered_backend)
    return registered_backend(&SEQUENCE, registered_context);
#endif
  return false;
}
