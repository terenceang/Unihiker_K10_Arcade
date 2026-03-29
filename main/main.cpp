#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"

static const char *TAG = "K10_MAIN";

#include "k10_state.h"
#include "k10_emulator.h"
#include "k10_hardware.h"
#include "k10_bt.h"
#include "k10_idf.h"
#include "k10_input.h"
#include "k10_video.h"

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
    k10_delay_ms(250);

    printf("\n");
    printf("Unihiker K10 Arcade\n");
    printf("This project is the pure ESP-IDF arcade target.\n");

    k10_hw_init();
    k10_bt_begin();

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
        uint8_t input_state = k10_read_inputs();

        // Integrate BLE Gamepad State
        k10_gamepad_state_t bt_state;
        k10_bt_get_gamepad_state(&bt_state);

        // BLE status — printed once per second
        static uint32_t last_bt_log_ms = 0;
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        if (now_ms - last_bt_log_ms >= 1000) {
            last_bt_log_ms = now_ms;
            if (bt_state.connected) {
                ESP_LOGI(TAG, "[BT] Connected  | Btns:%04x Hat:%d LX:%4d LY:%4d RX:%4d RY:%4d",
                         bt_state.buttons, bt_state.hat,
                         bt_state.lx, bt_state.ly, bt_state.rx, bt_state.ry);
            } else {
                ESP_LOGI(TAG, "[BT] Scanning...");
            }
        }

        if (bt_state.connected) {
            // Map LX/LY to Directions
            if (bt_state.lx < -40) input_state |= K10_BUTTON_LEFT;
            if (bt_state.lx > 40)  input_state |= K10_BUTTON_RIGHT;
            if (bt_state.ly < -40) input_state |= K10_BUTTON_UP;
            if (bt_state.ly > 40)  input_state |= K10_BUTTON_DOWN;

            // Map Buttons (Generic HID Gamepad bits often: 0=A, 1=B, 2=X, 3=Y)
            if (bt_state.buttons & 0x01) input_state |= K10_BUTTON_FIRE; // A -> Fire
            if (bt_state.buttons & 0x02) input_state |= K10_BUTTON_COIN; // B -> Coin
            if (bt_state.buttons & 0x08) input_state |= K10_BUTTON_START; // Y/X? -> Start (simplified)
        }

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

        if (input_state != last_input_state) {
            last_input_state = input_state;
        }

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