#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"

static const char *TAG = "K10_MAIN";

#include "k10_state.h"
#include "k10_emulator.h"
#include "k10_hardware.h"
#include "k10_wifi_gamepad.h"
#include "k10_idf.h"
#include "k10_input.h"
#include "k10_video.h"
#include "config/k10_config.h"

static uint8_t last_input_state = 0;
static TaskHandle_t g_main_task = nullptr;

static void start_current_machine() {
    if (!k10_emulator_supports_machine(k10_state_current_machine())) {
        printf("Machine not supported yet: %s\n", k10_state_current_name());
        return;
    }

    if (!k10_emulator_start(k10_state_current_machine())) {
        printf("Emulator start failed: %s\n", k10_state_current_name());
        return;
    }

    printf("Machine active: %s\n", k10_state_current_name());
}

static void k10_main_task(void* parameter) {
    (void)parameter;

    printf("\n");
    printf("Unihiker K10 Arcade v" K10_VERSION "\n");
    printf("This project is the pure ESP-IDF arcade target.\n");

    k10_hw_init();
    k10_wifi_gamepad_begin();

    if (k10_state_boot()) {
        if (k10_state_single_game_mode()) {
            printf("Single game boot: %s\n", k10_state_current_name());
            start_current_machine();
        } else {
            printf("Video menu active: %s\n", k10_state_current_name());
        }
    } else {
        printf("Video init failed.\n");
    }

    while (true) {
        uint8_t raw_input_state = k10_read_inputs();

        // Integrate WiFi Gamepad State
        k10_gamepad_state_t wifi_state;
        k10_wifi_gamepad_get_state(&wifi_state);

        uint8_t input_state = raw_input_state;
        if (wifi_state.lx < -40) input_state |= K10_BUTTON_LEFT;
        if (wifi_state.lx > 40)  input_state |= K10_BUTTON_RIGHT;
        if (wifi_state.ly < -40) input_state |= K10_BUTTON_UP;
        if (wifi_state.ly > 40)  input_state |= K10_BUTTON_DOWN;

        // Map POV Hat to Directions using a lookup table
        static const uint8_t hat_map[] = {
            K10_BUTTON_UP,                                // 0: N
            K10_BUTTON_UP | K10_BUTTON_RIGHT,             // 1: NE
            K10_BUTTON_RIGHT,                             // 2: E
            K10_BUTTON_DOWN | K10_BUTTON_RIGHT,           // 3: SE
            K10_BUTTON_DOWN,                              // 4: S
            K10_BUTTON_DOWN | K10_BUTTON_LEFT,            // 5: SW
            K10_BUTTON_LEFT,                              // 6: W
            K10_BUTTON_UP | K10_BUTTON_LEFT,              // 7: NW
            0                                             // 8: Center
        };
        if (wifi_state.hat < 9) {
            input_state |= hat_map[wifi_state.hat];
        }

        // Map Buttons (A=Fire, B=Coin, Start=Start, Coin=Extra)
        // wifi_state.buttons bitmasks already match K10_BUTTON_... enums in gamepad.html
        input_state |= (uint8_t)(wifi_state.buttons & 0xFF);


        switch (k10_state_handle_input(input_state, last_input_state)) {
            case K10_STATE_EVENT_MENU_CHANGED:
                printf("Menu: %s\n", k10_state_current_name());
                break;
            case K10_STATE_EVENT_LAUNCHED:
                start_current_machine();
                break;
            case K10_STATE_EVENT_RETURNED_TO_MENU:
                if (k10_emulator_is_running()) {
                    k10_emulator_stop();
                }
                printf("Return to menu: %s\n", k10_state_current_name());
                break;
            case K10_STATE_EVENT_RESTART_REQUESTED:
                if (k10_emulator_is_running()) {
                    k10_emulator_stop();
                }
                printf("Restart game: %s\n", k10_state_current_name());
                start_current_machine();
                break;
            case K10_STATE_EVENT_NONE:
            default:
                break;
        }

        last_input_state = input_state;

        if (!k10_state_in_menu() && k10_emulator_is_running()) {
            k10_emulator_run_frame(input_state);
            continue;
        }

        k10_delay_ms(5);
    }
}

static void start_main_task(void) {
    if (g_main_task != nullptr) {
        return;
    }

    xTaskCreatePinnedToCore(k10_main_task, "k10_main", 8192, nullptr, 2, &g_main_task, 1);
}

extern "C" void app_main(void) {
    start_main_task();
}