static inline void emulation_run_game_frame(void) {
#ifdef ENABLE_PACMAN
PACMAN_BEGIN
  pacman_run_frame();
PACMAN_END
#endif

#ifdef ENABLE_GALAGA
GALAGA_BEGIN
  galaga_run_frame();
GALAGA_END
#endif

#ifdef ENABLE_DKONG
DKONG_BEGIN
  dkong_run_frame();
DKONG_END
#endif

#ifdef ENABLE_FROGGER
FROGGER_BEGIN
  frogger_run_frame();
FROGGER_END
#endif

#ifdef ENABLE_DIGDUG
DIGDUG_BEGIN
  digdug_run_frame();
DIGDUG_END
#endif

#ifdef ENABLE_1942
_1942_BEGIN
  _1942_run_frame();
_1942_END
#endif
}
