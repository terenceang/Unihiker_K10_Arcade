#ifndef SINGLE_MACHINE
extern void hw_reset(void);

// return to main menu
void emulation_reset(void) {
#if 1
  hw_reset();
#else
  // return to menu
  machine = MCH_MENU;

  // reset all CPUs
  for(current_cpu=0;current_cpu<3;current_cpu++)
    ResetZ80(&cpu[current_cpu]);

#ifdef ENABLE_DKONG
  i8048_reset(&cpu_8048);
#endif
#endif
}
#endif

#ifdef ENABLE_1942
#define RAMSIZE   (8192 + 1024 + 128)
#else
#define RAMSIZE   (9216)
#endif

void prepare_emulation(void) {
  memory = (unsigned char *)heap_caps_malloc(RAMSIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  if (memory) {
    memset(memory, 0, RAMSIZE);
  }

#if defined(MASTER_ATTRACT_MENU_TIMEOUT) && !defined(SINGLE_MACHINE)
  master_attract_timeout = k10_millis();
#endif

  // reset interrupt enable flags so a previous game's state doesn't bleed in
  irq_enable[0] = irq_enable[1] = irq_enable[2] = 0;

  // reset all three z80's although we might not use them all
  for(current_cpu=0;current_cpu<3;current_cpu++)
    ResetZ80(&cpu[current_cpu]);

  current_cpu = 0;
#ifdef ENABLE_GALAGA
  current_rom_base = galaga_rom_cpu1;
#elif defined(ENABLE_PACMAN)
  current_rom_base = pacman_rom;
#elif defined(ENABLE_DIGDUG)
  current_rom_base = digdug_rom_cpu1;
#elif defined(ENABLE_FROGGER)
  current_rom_base = frogger_rom_cpu1;
#elif defined(ENABLE_DKONG)
  current_rom_base = dkong_rom_cpu1;
#elif defined(ENABLE_1942)
  current_rom_base = _1942_rom_cpu1;
#else
  current_rom_base = NULL;
#endif

#ifdef ENABLE_DKONG
  i8048_reset(&cpu_8048);
#endif

#if defined(ENABLE_GALAGA) || defined(ENABLE_DIGDUG) || defined(ENABLE_1942)
  // sub_cpu_reset starts at 1 (secondary CPU held in reset) on every game start
  sub_cpu_reset = 1;
#endif

#ifdef ENABLE_1942
  // Reset 1942-specific hardware registers to their power-on state.
  // These are global variables that persist between game sessions, so they
  // must be explicitly re-initialised here.
  _1942_bank        = 0;
  _1942_palette     = 0;
  _1942_scroll      = 0;
  _1942_sound_latch = 0;
  _1942_ay_addr[0]  = 0;
  _1942_ay_addr[1]  = 0;
#endif
}
