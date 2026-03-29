#ifndef K10_STATE_H
#define K10_STATE_H

#include <stdint.h>

enum K10Machine {
    K10_MACHINE_MENU = 0,
    K10_MACHINE_PACMAN,
    K10_MACHINE_GALAGA,
    K10_MACHINE_DKONG,
    K10_MACHINE_FROGGER,
    K10_MACHINE_DIGDUG,
    K10_MACHINE_1942,
    K10_MACHINE_LAST,
};

enum K10StateEvent {
    K10_STATE_EVENT_NONE = 0,
    K10_STATE_EVENT_MENU_CHANGED,
    K10_STATE_EVENT_LAUNCHED,
    K10_STATE_EVENT_RETURNED_TO_MENU,
    K10_STATE_EVENT_RESTART_REQUESTED,
};

bool k10_state_boot();
K10StateEvent k10_state_handle_input(uint8_t input_state, uint8_t last_input_state);
bool k10_state_single_game_mode();
bool k10_state_in_menu();
K10Machine k10_state_current_machine();
K10Machine k10_state_menu_selection();
const char* k10_state_current_name();

#endif