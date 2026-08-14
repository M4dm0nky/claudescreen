#ifndef PROJECT_STAR_CHIME_H
#define PROJECT_STAR_CHIME_H

#include <stdbool.h>
#include <stdint.h>

#define TK_PROJECT_STAR_CHIME_NOTE_COUNT 2

typedef struct {
  uint16_t frequency_hz;
  uint16_t duration_ms;
  uint16_t gap_after_ms;
  uint16_t attack_ms;
  uint16_t release_ms;
  uint8_t level_percent;
} tk_project_star_chime_note;

typedef struct {
  tk_project_star_chime_note notes[TK_PROJECT_STAR_CHIME_NOTE_COUNT];
} tk_project_star_chime_sequence;

typedef bool (*tk_project_star_chime_backend)(
    const tk_project_star_chime_sequence *sequence, void *context);

/* A platform audio owner may register a backend after it has proved that it
 * can borrow and release the codec/I2S resources without starving display
 * DMA. The callback is a non-blocking start/enqueue contract: it must return
 * before audio playback completes. With no verified backend, requests
 * intentionally fail closed. */
void tk_project_star_chime_set_backend(tk_project_star_chime_backend backend,
                                       void *context);
const tk_project_star_chime_sequence *tk_project_star_chime_get_sequence(void);
bool tk_project_star_chime_request(void);

#endif
