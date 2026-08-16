#pragma once
/*
 * LVGL:s TLSF-pool i PSRAM i stället för internt BSS (frysfix 2026-08-16, se
 * rot-CMakeLists och docs/lessons.md). LVGL:s inbyggda allokator lägger annars
 * sin LV_MEM_SIZE-pool som en statisk array i INTERNT BSS. På den här panelen
 * tryckte den poolen (+ Needs You-byggets objekt) det största SAMMANHÄNGANDE
 * DMA-dugliga interna blocket UNDER panelflushens 11 520 B (480×12×2) — då dör
 * varje flush i NO_MEM, LVGL-tasken fastnar i render medan den håller
 * LVGL-låset, och HELA glaset fryser bakom det (samma minnesklass som
 * frysläxan 2026-08-14, annan svältkälla).
 *
 * Rot-CMakeLists sätter LV_MEM_POOL_INCLUDE till den här filen mot
 * lvgl-komponenten; lv_mem_core_builtin.c inkluderar den och ber sedan om
 * poolen via makrot nedan i stället för den statiska arrayen. Poolen tas EN
 * gång vid lv_mem_init (tidigt i display_start; PSRAM är uppe sedan boot). Det
 * interna heapet får då tillbaka hela poolen som sammanhängande block, så
 * DMA-blocket ligger tryggt över flushtröskeln.
 *
 * Makrot (inte en -D-flagga): CMake släpper funktionslika definitioner på
 * kommandoraden, så det måste bo i en header. PSRAM + 8BIT, INTE DMA — LVGL:s
 * objekt/ritlager behöver inte vara DMA-dugliga; hela poängen är att lämna
 * kvar det interna DMA-minnet åt panelflushen.
 */
#include "esp_heap_caps.h"

#define LV_MEM_POOL_ALLOC(size) \
  heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
