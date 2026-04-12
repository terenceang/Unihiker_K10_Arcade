/*
 * emulation.c
 *
 */

/*
 * TODO: 
 * Don't use reset to return to menu in order to suppress noise when returning from master attract game
 * 
 */

#include <stdio.h>    // for printf
#include <string.h>   // for memcpy

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_random.h>

#include "k10_idf.h"
#include "k10_hardware.h"

#include "cpu/z80/Z80.h"
#include "config.h"

#define CPU_EMULATION
#include "emulation.h"

#ifdef ENABLE_DKONG
#include "cpu/i8048/i8048.h"
#include "games/dkong/dkong_rom2.h"
#endif

#include "emulation/emulation_state.inc"

#include "emulation/emulation_digdug_dkong.inc"

// one inst at 3Mhz ~ 500k inst/sec = 500000/60 inst per frame
#define INST_PER_FRAME 300000/60/4

#ifdef ENABLE_PACMAN
#include "games/pacman/pacman.h"
#endif

#ifdef ENABLE_GALAGA
#include "games/galaga/galaga.h"
#endif

#ifdef ENABLE_DKONG
#include "games/dkong/dkong.h"
#endif

#ifdef ENABLE_FROGGER
#include "games/frogger/frogger.h"
#endif

#ifdef ENABLE_DIGDUG
#include "games/digdug/digdug.h"
#endif

#ifdef ENABLE_1942
#include "games/_1942/1942.h"
#endif


#include "emulation/emulation_cpu_hooks.inc"
#include "emulation/emulation_menu.inc"
#include "emulation/emulation_run_games.inc"
#include "emulation/emulation_orchestration.inc"
#include "emulation/emulation_lifecycle.inc"
#include "emulation/emulation_frame_sync.inc"

void emulate_frame(void) {
  current_cpu = 0;

  emulation_run_orchestration_frame();

  // It may happen that the emulation runs too slow. It will then miss the
  // vblank notification and in turn will miss a frame and significantly
  // slow down. This risk is only given with Galaga as the emulation of
  // all three CPUs takes nearly 13ms. The 60hz vblank rate is in turn 
  // 16.6 ms.

  emulation_sync_frame_timing();
}
