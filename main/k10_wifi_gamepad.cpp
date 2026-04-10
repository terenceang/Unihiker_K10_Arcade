// k10_wifi_gamepad.cpp — Simple WiFi gamepad receiver for ESP32
//
// This module starts a WiFi AP and web server. Users connect via phone and open a web page
// with a virtual gamepad. Gamepad state is sent to the ESP32 via HTTP POST (JSON) or WebSocket.
//
// Minimal implementation: HTTP POST endpoint for /gamepad, JSON body: { "buttons": 0, "hat": 0, "lx": 0, "ly": 0, "rx": 0, "ry": 0 }
//
// Dependencies: ESP-IDF v5.x, ESP32 WiFi, ESP Async Web Server (or IDF native HTTP server)

#include "k10_input.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "K10_WIFI";

static k10_gamepad_state_t g_state = {0};
static SemaphoreHandle_t g_mutex = NULL;

// HTTP POST handler for /gamepad
static esp_err_t gamepad_post_handler(httpd_req_t *req) {
    if (req->content_len < 7) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Too short");
        return ESP_FAIL;
    }

    uint8_t buf[8];
    int ret = httpd_req_recv(req, (char*)buf, 7);
    if (ret <= 0) return ESP_FAIL;

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Binary format: [btn_lo, btn_hi, hat, lx, ly, rx, ry]
        g_state.buttons = buf[0] | (buf[1] << 8);
        g_state.hat     = buf[2];
        g_state.lx      = (int8_t)buf[3];
        g_state.ly      = (int8_t)buf[4];
        g_state.rx      = (int8_t)buf[5];
        g_state.ry      = (int8_t)buf[6];
        g_state.connected = true;
        xSemaphoreGive(g_mutex);
    }

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

// Serve the gamepad web page (for root, captive, or redirect)
static esp_err_t gamepad_html_handler(httpd_req_t *req) {
    #include "gamepad_html.h"
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, gamepad_html, gamepad_html_len);
    return ESP_OK;
}

// Captive portal: redirect all unknown URLs to the gamepad page
static esp_err_t captive_redirect_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "Redirecting to / ...");
    return ESP_OK;
}

// Captive portal: serve gamepad page for common captive endpoints
static esp_err_t captive_gamepad_handler(httpd_req_t *req) {
    return gamepad_html_handler(req);
}

// Start WiFi AP and HTTP server
void k10_wifi_gamepad_begin(void) {
    if (g_mutex == NULL) {
        g_mutex = xSemaphoreCreateMutex();
    }
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "K10-Gamepad",
            .password = "12345678",
            .ssid_len = 0,
            .channel = 1,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = 0,
            .max_connection = 4,
            .beacon_interval = 100,
            .pairwise_cipher = WIFI_CIPHER_TYPE_TKIP_CCMP,
            .ftm_responder = 0,
            .pmf_cfg = { .capable = true, .required = false },
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi AP started: SSID=K10-Gamepad, password=12345678");
    // HTTP server
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &server_config));
    httpd_uri_t gamepad_uri = {
        .uri = "/gamepad",
        .method = HTTP_POST,
        .handler = gamepad_post_handler
    };
    httpd_register_uri_handler(server, &gamepad_uri);

    // Serve gamepad page at root
    httpd_uri_t html_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = gamepad_html_handler
    };
    httpd_register_uri_handler(server, &html_uri);

    // Captive portal endpoints (Android, iOS, Windows)
    httpd_uri_t captive1 = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = captive_gamepad_handler
    };
    httpd_register_uri_handler(server, &captive1);
    httpd_uri_t captive2 = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = captive_gamepad_handler
    };
    httpd_register_uri_handler(server, &captive2);
    httpd_uri_t captive3 = {
        .uri = "/ncsi.txt",
        .method = HTTP_GET,
        .handler = captive_gamepad_handler
    };
    httpd_register_uri_handler(server, &captive3);

    // Catch-all: redirect all other GET requests to root
    httpd_uri_t catchall = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = captive_redirect_handler
    };
    httpd_register_uri_handler(server, &catchall);
}

void k10_wifi_gamepad_get_state(k10_gamepad_state_t *state) {
    if (!state || !g_mutex) return;
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        memcpy(state, &g_state, sizeof(k10_gamepad_state_t));
        xSemaphoreGive(g_mutex);
    }
}
