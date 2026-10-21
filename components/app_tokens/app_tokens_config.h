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

/* Direct mode: the panel owns a Claude login and needs no Mac, so every page
 * that only the Mac's local logs can fill (burn rate, trackers, value, Codex,
 * GitHub) is not built at all. The four that remain are the quota pages and
 * the dollar credits. Overridable so the simulator can render this layout. */
#ifndef TK_DIRECT_MODE
#ifdef TK_CLAUDE_REFRESH_TOKEN
#define TK_DIRECT_MODE 1
#else
#define TK_DIRECT_MODE 0
#endif
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
