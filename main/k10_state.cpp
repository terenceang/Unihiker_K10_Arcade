#include "k10_state.h"

#include <string.h>

#include "k10_hardware.h"
#include "k10_input.h"
#include "k10_video.h"

#include "arcade_core/config.h"

namespace {

#ifdef ENABLE_GALAGA
constexpr K10Machine kDefaultMachine = K10_MACHINE_GALAGA;
#elif defined(ENABLE_PACMAN)
constexpr K10Machine kDefaultMachine = K10_MACHINE_PACMAN;
#else
constexpr K10Machine kDefaultMachine = K10_MACHINE_MENU;
#endif

K10Machine g_machine = K10_MACHINE_MENU;
int g_menu_index = 0;

bool single_game_mode_enabled() {
    return false; // Could be driven by a macro if needed
}

bool pressed(uint8_t input_state, uint8_t last_input_state, uint8_t mask) {
    return (input_state & mask) != 0 && (last_input_state & mask) == 0;
}

bool machine_uses_dkong_rate(K10Machine machine) {
    return machine == K10_MACHINE_DKONG;
}

void render_current_view() {
    if (g_machine == K10_MACHINE_MENU) {
        k10_video_draw_menu_frame(g_menu_index);
    } else {
        k10_video_draw_machine_frame(static_cast<int>(g_machine));
    }
}

}  // namespace

bool k10_state_boot() {
    if (!k10_video_begin()) {
        return false;
    }

    g_menu_index = 0;
    // Find index of default machine if possible
    for (int i = 0; i < 10; ++i) {
        if (k10_video_menu_name(i) && strcmp(k10_video_menu_name(i), "Galaga") == 0) {
            g_menu_index = i;
            break;
        }
    }

    g_machine = single_game_mode_enabled() ? kDefaultMachine : K10_MACHINE_MENU;
    k10_audio_set_dkong_rate(machine_uses_dkong_rate(g_machine));
    render_current_view();
    return true;
}

K10StateEvent k10_state_handle_input(uint8_t input_state, uint8_t last_input_state) {
    if (single_game_mode_enabled() && g_machine != K10_MACHINE_MENU) {
        if ((input_state & K10_BUTTON_START) && (input_state & K10_BUTTON_COIN) &&
            !((last_input_state & K10_BUTTON_START) && (last_input_state & K10_BUTTON_COIN))) {
            return K10_STATE_EVENT_RESTART_REQUESTED;
        }

        return K10_STATE_EVENT_NONE;
    }

    if (g_machine == K10_MACHINE_MENU) {
        if (pressed(input_state, last_input_state, K10_BUTTON_UP) ||
            pressed(input_state, last_input_state, K10_BUTTON_LEFT)) {
            g_menu_index = k10_video_wrap_menu_selection(g_menu_index, -1);
            render_current_view();
            return K10_STATE_EVENT_MENU_CHANGED;
        }

        if (pressed(input_state, last_input_state, K10_BUTTON_DOWN) ||
            pressed(input_state, last_input_state, K10_BUTTON_RIGHT)) {
            g_menu_index = k10_video_wrap_menu_selection(g_menu_index, 1);
            render_current_view();
            return K10_STATE_EVENT_MENU_CHANGED;
        }

        if (pressed(input_state, last_input_state, K10_BUTTON_FIRE) ||
            pressed(input_state, last_input_state, K10_BUTTON_START)) {
            g_machine = k10_video_menu_machine(g_menu_index);
            k10_audio_set_dkong_rate(machine_uses_dkong_rate(g_machine));
            render_current_view();
            return K10_STATE_EVENT_LAUNCHED; 
        }

        return K10_STATE_EVENT_NONE;
        }

        if (g_machine == K10_MACHINE_GALAGA) {
        if ((input_state & K10_BUTTON_START) && (input_state & K10_BUTTON_COIN) &&
            !((last_input_state & K10_BUTTON_START) && (last_input_state & K10_BUTTON_COIN))) {
            g_machine = K10_MACHINE_MENU;
            k10_audio_set_dkong_rate(false);
            render_current_view();
            return K10_STATE_EVENT_RETURNED_TO_MENU;
        }

        return K10_STATE_EVENT_NONE;
        }

        if (pressed(input_state, last_input_state, K10_BUTTON_COIN)) {
        g_machine = K10_MACHINE_MENU;
        k10_audio_set_dkong_rate(false);
        render_current_view();
        return K10_STATE_EVENT_RETURNED_TO_MENU;
        }

        return K10_STATE_EVENT_NONE;
        }

        bool k10_state_single_game_mode() {
        return single_game_mode_enabled();
        }

        bool k10_state_in_menu() {
        return g_machine == K10_MACHINE_MENU;
        }

        K10Machine k10_state_current_machine() {
        return g_machine;
        }

        K10Machine k10_state_menu_selection() {
        return k10_video_menu_machine(g_menu_index);
        }

        const char* k10_state_current_name() {
        if (g_machine == K10_MACHINE_MENU) {
        return k10_video_menu_name(g_menu_index);
        }

        return k10_video_menu_name(g_menu_index); // This index matches the machine launched
        }