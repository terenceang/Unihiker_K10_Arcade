#ifndef SINGLE_MACHINE
static inline void emulation_handle_menu_frame(void) {
  static unsigned char last_mask = 0;

  // get a mask of currently pressed keys
  unsigned char keymask = buttons_get();

  // any key will cancel the master attract mode and
  // the machine stays in the menu forever once the
  // user has touched it
  if(keymask)
    master_attract_timeout = 0;

  if((keymask & BUTTON_UP) && !(last_mask & BUTTON_UP))
    menu_sel--;

  if((keymask & BUTTON_DOWN) && !(last_mask & BUTTON_DOWN))
    menu_sel++;

  if(((keymask & BUTTON_FIRE) && !(last_mask & BUTTON_FIRE)) ||
     ((keymask & BUTTON_START) && !(last_mask & BUTTON_START))) {
    machine = menu_sel;

#if defined(ENABLE_DKONG)
    // dkong uses a different sample rate,
    // so reconfigure audio if dkong is requested
    audio_dkong_bitrate(machine == MCH_DKONG);
#endif
  }

  if(MACHINES <= 3) {
    if(menu_sel < 1)        menu_sel = 1;
    if(menu_sel > MACHINES) menu_sel = MACHINES;
  } else {
    if(menu_sel < 1)        menu_sel = MACHINES;
    if(menu_sel > MACHINES) menu_sel = 1;
  }

  last_mask = keymask;

#ifdef MASTER_ATTRACT_MENU_TIMEOUT
  // check for master attract timeout
  if(master_attract_timeout &&
     (k10_millis() - master_attract_timeout > MASTER_ATTRACT_MENU_TIMEOUT)) {
    master_attract_timeout = k10_millis();  // new timeout for running game

    // randomly select a machine
    machine = 1 + ((unsigned long)esp_random()) % MACHINES;
    printf("MASTER ATTRACT to machine %d!!!\n", machine);

#ifdef ENABLE_DKONG
    // dkong uses a different sample rate, so reconfigure audio if
    // kong is requested
    audio_dkong_bitrate(machine == MCH_DKONG);
#endif
  }
#endif
}
#endif
