static inline void emulation_sync_frame_timing(void) {
  // Wait for signal from video task to emulate a 60Hz frame rate. Don't do
  // this unless the game has actually started to speed up the boot process
  // a little bit.
  if(game_started
#ifndef SINGLE_MACHINE
    || (machine == MCH_MENU)
#endif
    )
    ulTaskNotifyTake(1, 0xffffffffUL);
  else
    // give a millisecond delay to make the watchdog happy
    vTaskDelay(1);

#ifdef MASTER_ATTRACT_GAME_TIMEOUT
  if((machine != MCH_MENU) && master_attract_timeout && (k10_millis() - master_attract_timeout > MASTER_ATTRACT_GAME_TIMEOUT)) {
    printf("MASTER ATTRACT game timeout, return to menu\n");
    master_attract_timeout = k10_millis();
    emulation_reset();
  }
#endif
}
