#ifndef APP_TOKENS_CONFIG_H
#define APP_TOKENS_CONFIG_H

#ifdef ESP_PLATFORM
#include "secrets.h"
#endif

/* The GitHub page and star popup are deliberately independent. A fresh clone
 * remains Claude/Codex-only until the user opts in through secrets.h. */
#ifndef TK_GITHUB_SCREEN_ENABLED
#define TK_GITHUB_SCREEN_ENABLED 0
#endif

#ifndef TK_GITHUB_NOTIFICATIONS_ENABLED
#define TK_GITHUB_NOTIFICATIONS_ENABLED 0
#endif

/* Sound is a third, independent opt-in. It still requires a platform backend
 * that has passed the display-DMA and physical-speaker gates. */
#ifndef TK_GITHUB_SOUND_ENABLED
#define TK_GITHUB_SOUND_ENABLED 0
#endif

#if TK_GITHUB_SCREEN_ENABLED != 0 && TK_GITHUB_SCREEN_ENABLED != 1
#error "TK_GITHUB_SCREEN_ENABLED must be 0 or 1"
#endif

#if TK_GITHUB_NOTIFICATIONS_ENABLED != 0 && \
    TK_GITHUB_NOTIFICATIONS_ENABLED != 1
#error "TK_GITHUB_NOTIFICATIONS_ENABLED must be 0 or 1"
#endif

#if TK_GITHUB_SOUND_ENABLED != 0 && TK_GITHUB_SOUND_ENABLED != 1
#error "TK_GITHUB_SOUND_ENABLED must be 0 or 1"
#endif

#endif
