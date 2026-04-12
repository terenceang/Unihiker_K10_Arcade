#include "k10_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs.h"
#include "esp_random.h"
#include "esp_log.h"

#include "k10_hardware.h"
#include "k10_input.h"
#include "k10_video.h"

#include "arcade_core/config.h"
#include "config/k10_game_registry.h"

static const char *TAG = "K10_STATE";

// ── Idle-launch timeout ────────────────────────────────────────────────────────
// If no gamepad/button input is received for this many milliseconds while the
// menu is showing, a random game is launched automatically.
// Change this value to adjust the delay.
#define K10_IDLE_LAUNCH_TIMEOUT_MS  30000

// ── Attract rotation ───────────────────────────────────────────────────────────
// When a game was idle-launched (no user selection), return to the menu after
// this many milliseconds and let the idle timer pick another random game.
#define K10_ATTRACT_ROTATE_MS  60000

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

constexpr K10Machine kDefaultMachine = k10_default_enabled_machine();

K10Machine g_machine   = K10_MACHINE_MENU;
int        g_menu_index = 0;

// Idle-launch timer state
uint32_t   g_last_input_ms  = 0;   // time of last button activity in menu
bool       g_idle_active    = false;   // true while the idle countdown is running

// Attract rotation: when true the current game was auto-launched (not user-selected)
// and should rotate to another random game after K10_ATTRACT_ROTATE_MS.
bool       g_idle_launched  = false;
uint32_t   g_game_start_ms  = 0;

// Single-game restart guard: require a deliberate hold and enforce cooldown
// so gameplay button chatter doesn't repeatedly restart and flicker the screen.
uint32_t   g_restart_combo_start_ms = 0;
uint32_t   g_last_restart_ms = 0;
bool       g_restart_combo_latched = false;

constexpr uint32_t kRestartHoldMs = 500;
constexpr uint32_t kRestartCooldownMs = 1500;

static uint32_t now_ms() {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

bool single_game_mode_enabled() {
#ifdef SINGLE_MACHINE
    return true;
#else
    return false;
#endif
}

bool pressed(uint8_t input_state, uint8_t last_input_state, uint8_t mask) {
    return (input_state & mask) != 0 && (last_input_state & mask) == 0;
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
    k10_audio_set_dkong_rate(k10_machine_uses_dkong_audio_rate(g_machine));
    nvs_save_last_selection(g_menu_index);
    render_current_view();
    return K10_STATE_EVENT_LAUNCHED;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────

bool k10_state_boot() {
    if (!k10_video_begin()) return false;

    const bool single_mode = single_game_mode_enabled();

    // Restore last selection from NVS; fall back to the compiled default.
    int saved = single_mode ? -1 : nvs_load_last_selection();
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

    g_machine = single_mode ? kDefaultMachine : K10_MACHINE_MENU;
    k10_audio_set_dkong_rate(k10_machine_uses_dkong_audio_rate(g_machine));
    reset_idle_timer();
    if (!single_mode) {
        render_current_view();
    }
    return true;
}

K10StateEvent k10_state_handle_input(uint8_t input_state, uint8_t last_input_state) {
    // ── Single-game mode: only restart combo matters ──────────────────────────
    if (single_game_mode_enabled() && g_machine != K10_MACHINE_MENU) {
        const bool combo_now = (input_state & K10_BUTTON_START) && (input_state & K10_BUTTON_COIN);
        const uint32_t now = now_ms();

        if (!combo_now) {
            g_restart_combo_start_ms = 0;
            g_restart_combo_latched = false;
            return K10_STATE_EVENT_NONE;
        }

        if (g_restart_combo_start_ms == 0) {
            g_restart_combo_start_ms = now;
            return K10_STATE_EVENT_NONE;
        }

        if (g_restart_combo_latched) {
            return K10_STATE_EVENT_NONE;
        }

        if ((now - g_restart_combo_start_ms) >= kRestartHoldMs &&
            (now - g_last_restart_ms) >= kRestartCooldownMs) {
            g_restart_combo_latched = true;
            g_last_restart_ms = now;
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

        // Launch selected game — FIRE, START, or COIN (arcade-style "insert coin to play")
        if (pressed(input_state, last_input_state, K10_BUTTON_FIRE) ||
            pressed(input_state, last_input_state, K10_BUTTON_START) ||
            pressed(input_state, last_input_state, K10_BUTTON_COIN)) {
            g_idle_active = false;
            g_idle_launched = false;
            g_game_start_ms = now_ms();
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
                g_idle_launched = true;
                g_game_start_ms = now_ms();
                ESP_LOGI(TAG, "Idle timeout — launching random game: %s",
                         k10_video_menu_name(g_menu_index));
                return launch_current_selection();
            }
        }

        return K10_STATE_EVENT_NONE;
    }

    // ── Attract rotation: auto-launched games cycle back to menu ───────────────
    if (g_idle_launched) {
        // Any user input while in attract mode claims the game (stop rotating)
        if (input_state != 0) {
            g_idle_launched = false;
        } else if ((now_ms() - g_game_start_ms) >= K10_ATTRACT_ROTATE_MS) {
            ESP_LOGI(TAG, "Attract rotate — returning to menu");
            g_idle_launched = false;
            g_machine = K10_MACHINE_MENU;
            k10_audio_set_dkong_rate(false);
            reset_idle_timer();
            render_current_view();
            return K10_STATE_EVENT_RETURNED_TO_MENU;
        }
    }

    // ── In-game: return-to-menu combos ────────────────────────────────────────
    // All games: START+COIN together returns to menu (same as Galaga).
    // This ensures COIN alone can be passed through to the game for inserting credits.
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
