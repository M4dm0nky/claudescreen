#include <stdio.h>

#include "../components/app_tokens/project_star_chime.h"

static int failures;
static int backend_calls;

static void check(const char *what, int condition) {
  if (!condition) {
    printf("FAIL %s\n", what);
    failures++;
  }
}

static bool backend(const tk_project_star_chime_sequence *sequence,
                    void *context) {
  backend_calls++;
  check("context is preserved", context == &failures);
  check("first note is A5", sequence->notes[0].frequency_hz == 880);
  check("second note is C sharp 6",
        sequence->notes[1].frequency_hz == 1109);
  check("chime stays below 300 ms",
        sequence->notes[0].duration_ms +
        sequence->notes[0].gap_after_ms +
        sequence->notes[1].duration_ms <= 300);
  check("gain request is restrained",
        sequence->notes[0].level_percent <= 15 &&
        sequence->notes[1].level_percent <= 15);
  return true;
}

int main(void) {
  tk_project_star_chime_set_backend(NULL, NULL);
  check("missing backend safely no-ops", !tk_project_star_chime_request());

  tk_project_star_chime_set_backend(backend, &failures);
  check("registered backend plays", tk_project_star_chime_request());
  check("one request means one playback", backend_calls == 1);

  tk_project_star_chime_set_backend(NULL, &backend_calls);
  check("unregister restores no-op", !tk_project_star_chime_request());

  if (failures) return 1;
  printf("GitHub star chime contract tests passed\n");
  return 0;
}
