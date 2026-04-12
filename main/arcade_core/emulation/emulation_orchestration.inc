// Orchestrates menu handling and per-machine frame execution.
static inline void emulation_run_orchestration_frame(void) {
#ifndef SINGLE_MACHINE
  if(machine == MCH_MENU) {
    emulation_handle_menu_frame();
  } else {
    emulation_run_game_frame();
  }
#else
  emulation_run_game_frame();
#endif
}
