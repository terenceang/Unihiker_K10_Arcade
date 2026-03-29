#include "k10_state.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs.h"
#include "esp_random.h"
#include "esp_log.h"

#include "k10_hardware.h"
#include "k10_input.h"
#include "k10_video.h"

#include "arcade_core/config.h"

static const char *TAG = "K10_STATE";

// ── Idle-launch timeout ────────────────────────────────────────────────────────
// If no gamepad/button input is received for this many milliseconds while the
// menu is showing, a random game is launched automatically.
// Change this value to adjust the delay.
#define K10_IDLE_LAUNCH_TIMEOUT_MS  3000

// ── NVS persistence ────────────────────────────────────────────────────────────
static const char *kNvsNamespace = "k10_state";
static const char *kNvsKeyLastSel = "last_sel";

static int nvs_load_last_selection() {
    nvs_handle_t h;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &h) != ESP_OK) return -1;
    int32_t idx = -1;
    nvs_get_i32(h, kNvsKeyLastSel, &idx);
    nvs_close(h);
    return (int)idx;
}

static void nvs_save_last_selection(int idx) {
    nvs_handle_t h;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed — selection not saved");
        return;
    }
    nvs_set_i32(h, kNvsKeyLastSel, (int32_t)idx);
    nvs_commit(h);
    nvs_close(h);
}

namespace {

#ifdef ENABLE_GALAGA
constexpr K10Machine kDefaultMachine = K10_MACHINE_GALAGA;
#elif defined(ENABLE_PACMAN)
constexpr K10Machine kDefaultMachine = K10_MACHINE_PACMAN;
#else
constexpr K10Machine kDefaultMachine = K10_MACHINE_MENU;
#endif

K10Machine g_machine   = K10_MACHINE_MENU;
int        g_menu_index = 0;

// Idle-launch timer state
uint32_t   g_last_input_ms  = 0;   // time of last button activity in menu
bool       g_idle_active    = false;   // true while the idle countdown is running

static uint32_t now_ms() {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

bool single_game_mode_enabled() {
    return false;
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

// Called whenever we return to the menu so the idle timer starts fresh.
void reset_idle_timer() {
    g_last_input_ms = now_ms();
    g_idle_active   = true;
}

// Launch the game at g_menu_index and persist the selection.
K10StateEvent launch_current_selection() {
    g_machine = k10_video_menu_machine(g_menu_index);
    k10_audio_set_dkong_rate(machine_uses_dkong_rate(g_machine));
    nvs_save_last_selection(g_menu_index);
    render_current_view();
    return K10_STATE_EVENT_LAUNCHED;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────

bool k10_state_boot() {
    if (!k10_video_begin()) return false;

    // Restore last selection from NVS; fall back to the compiled default.
    int saved = nvs_load_last_selection();
    int count = k10_video_menu_count();

    if (saved >= 0 && saved < count) {
        g_menu_index = saved;
        ESP_LOGI(TAG, "Restored last selection: %d (%s)", g_menu_index,
                 k10_video_menu_name(g_menu_index));
    } else {
        // No saved value — find the default machine in the menu list
        g_menu_index = 0;
        for (int i = 0; i < count; ++i) {
            if (k10_video_menu_machine(i) == kDefaultMachine) {
                g_menu_index = i;
                break;
            }
        }
    }

    g_machine = single_game_mode_enabled() ? kDefaultMachine : K10_MACHINE_MENU;
    k10_audio_set_dkong_rate(machine_uses_dkong_rate(g_machine));
    reset_idle_timer();
    render_current_view();
    return true;
}

K10StateEvent k10_state_handle_input(uint8_t input_state, uint8_t last_input_state) {
    // ── Single-game mode: only restart combo matters ──────────────────────────
    if (single_game_mode_enabled() && g_machine != K10_MACHINE_MENU) {
        if ((input_state & K10_BUTTON_START) && (input_state & K10_BUTTON_COIN) &&
            !((last_input_state & K10_BUTTON_START) && (last_input_state & K10_BUTTON_COIN))) {
            return K10_STATE_EVENT_RESTART_REQUESTED;
        }
        return K10_STATE_EVENT_NONE;
    }

    // ── Menu mode ─────────────────────────────────────────────────────────────
    if (g_machine == K10_MACHINE_MENU) {
        // Any button press restarts the idle countdown
        if (input_state != 0) {
            g_last_input_ms = now_ms();
            g_idle_active   = true;
        }

        // Navigate up / left
        if (pressed(input_state, last_input_state, K10_BUTTON_UP) ||
            pressed(input_state, last_input_state, K10_BUTTON_LEFT)) {
            g_menu_index = k10_video_wrap_menu_selection(g_menu_index, -1);
            render_current_view();
            return K10_STATE_EVENT_MENU_CHANGED;
        }

        // Navigate down / right
        if (pressed(input_state, last_input_state, K10_BUTTON_DOWN) ||
            pressed(input_state, last_input_state, K10_BUTTON_RIGHT)) {
            g_menu_index = k10_video_wrap_menu_selection(g_menu_index, 1);
            render_current_view();
            return K10_STATE_EVENT_MENU_CHANGED;
        }

        // Launch selected game
        if (pressed(input_state, last_input_state, K10_BUTTON_FIRE) ||
            pressed(input_state, last_input_state, K10_BUTTON_START)) {
            g_idle_active = false;
            return launch_current_selection();
        }

        // ── Idle-launch: no input for K10_IDLE_LAUNCH_TIMEOUT_MS ─────────────
        if (g_idle_active &&
            (now_ms() - g_last_input_ms) >= K10_IDLE_LAUNCH_TIMEOUT_MS) {
            g_idle_active = false;

            int count = k10_video_menu_count();
            if (count > 0) {
                // Pick a random entry; avoid repeating the current highlight
                // when there is more than one choice.
                int pick = (int)(esp_random() % (uint32_t)count);
                if (count > 1 && pick == g_menu_index) {
                    pick = (pick + 1) % count;
                }
                g_menu_index = pick;
                ESP_LOGI(TAG, "Idle timeout — launching random game: %s",
                         k10_video_menu_name(g_menu_index));
                return launch_current_selection();
            }
        }

        return K10_STATE_EVENT_NONE;
    }

    // ── In-game: return-to-menu combos ────────────────────────────────────────
    if (g_machine == K10_MACHINE_GALAGA) {
        if ((input_state & K10_BUTTON_START) && (input_state & K10_BUTTON_COIN) &&
            !((last_input_state & K10_BUTTON_START) && (last_input_state & K10_BUTTON_COIN))) {
            g_machine = K10_MACHINE_MENU;
            k10_audio_set_dkong_rate(false);
            reset_idle_timer();
            render_current_view();
            return K10_STATE_EVENT_RETURNED_TO_MENU;
        }
        return K10_STATE_EVENT_NONE;
    }

    if (pressed(input_state, last_input_state, K10_BUTTON_COIN)) {
        g_machine = K10_MACHINE_MENU;
        k10_audio_set_dkong_rate(false);
        reset_idle_timer();
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
    return k10_video_menu_name(g_menu_index);
}
