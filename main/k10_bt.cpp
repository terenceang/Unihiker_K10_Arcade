// ============================================================
// k10_bt.cpp — Production BLE HID Host for 8BitDo SN30 Pro
//
// Target  : ESP32-S3, ESP-IDF v5.x, NimBLE stack
// Role    : BLE Central (HID Host only — no peripheral code)
//
// Pairing (FIRST TIME):
//   Hold START + Y for 3 seconds on the controller.
//   It enters Switch BLE mode (LED flashes quickly).
//   The ESP32 pairs automatically — no user interaction needed.
//
// Reconnect (after reboot or sleep):
//   Press START only.
//   The ESP32 finds the bonded controller and reconnects.
//
// sdkconfig requirements (set in menuconfig or sdkconfig.defaults):
//   CONFIG_BT_NIMBLE_NVS_PERSIST=y          ← REQUIRED for bond persistence
//   CONFIG_BT_NIMBLE_MAX_BONDS=3             (at least 1)
//   CONFIG_BT_NIMBLE_SECURITY_ENABLE=y       (already set)
//   CONFIG_BT_NIMBLE_SM_SC=y                 (already set)
//   CONFIG_BT_NIMBLE_ROLE_CENTRAL=y          (already set)
// ============================================================

#include "k10_bt.h"

#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
// ble_store_config_init() is defined in the IDF component but not declared in
// any public header — forward-declare it exactly as the IDF examples do.
extern "C" void ble_store_config_init(void);

static const char *TAG = "K10_BT";

// ── UUIDs ─────────────────────────────────────────────────────────────────────
static const ble_uuid16_t kUuidHidSvc = BLE_UUID16_INIT(0x1812);
static const ble_uuid16_t kUuidReport = BLE_UUID16_INIT(0x2A4D);
static const ble_uuid16_t kUuidCccd   = BLE_UUID16_INIT(0x2902);

// ── Tuning constants ───────────────────────────────────────────────────────────
#define TARGET_NAME          "8BitDo"       // substring match on advertised name
#define MAX_REPORT_CHRS      8              // max 0x2A4D characteristics tracked
#define SCAN_ITVL_MS         100            // scan interval
#define SCAN_WIN_MS          50             // scan window
#define CONN_TIMEOUT_MS      10000          // give up connecting after 10 s

// ── Per-Report-characteristic tracking ───────────────────────────────────────
typedef struct {
    uint16_t val_handle;   // attribute handle — notifications arrive here
    uint16_t end_handle;   // upper bound for descriptor range scan
    uint16_t cccd_handle;  // CCCD attribute handle once found; 0 = not found yet
} report_chr_t;

// ── Module-level state ─────────────────────────────────────────────────────────
static uint16_t          g_conn_handle  = BLE_HS_CONN_HANDLE_NONE;
static uint8_t           g_own_addr_type;

// Report characteristic array, populated during GATT discovery
static report_chr_t      g_chrs[MAX_REPORT_CHRS];
static int               g_chr_count = 0;  // total Report chrs found
static uint16_t          g_svc_end   = 0;  // HID service end handle

// Shared gamepad state — written by BLE task, read by main task
static SemaphoreHandle_t g_state_mutex = NULL;
static k10_gamepad_state_t g_state     = {};

// ── Forward declarations ───────────────────────────────────────────────────────
static void start_scan(void);
static void start_gatt_discovery(void);
static void begin_dsc_discovery(int idx);
static int  gap_event_cb(struct ble_gap_event *event, void *arg);


// =============================================================================
//  Report decoding — 8BitDo SN30 Pro in Switch BLE mode
// =============================================================================
//
//  Switch-mode reports may be prefixed with a Report ID byte.
//  Heuristic: if len == 7, parse from byte 0; if len >= 8, skip byte 0 (report ID).
//
//  Layout (after optional Report ID):
//    [0] buttons_lo   [1] buttons_hi   [2] hat (nibble 0x0–0x8)
//    [3] LX           [4] LY           [5] RX   [6] RY
//  Sticks are 0x00–0xFF, centre = 0x80; we shift to signed –128..+127.

static void decode_report(const uint8_t *d, uint16_t len)
{
    const uint8_t *b;
    if      (len >= 8) { b = d + 1; }   // skip report ID byte
    else if (len >= 7) { b = d; }
    else               { return; }      // too short to decode

    k10_gamepad_state_t s  = {};
    s.connected = true;
    s.buttons   = (uint16_t)((b[1] << 8) | b[0]);
    s.hat       = b[2] & 0x0F;          // 0-7 = directions, 8 = centred
    s.lx        = (int8_t)(b[3] - 0x80);
    s.ly        = (int8_t)(b[4] - 0x80);
    s.rx        = (int8_t)(b[5] - 0x80);
    s.ry        = (int8_t)(b[6] - 0x80);

    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state = s;
    xSemaphoreGive(g_state_mutex);

    // Throttled log at ~50 Hz
    static uint32_t last_log_ms = 0;
    uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (now_ms - last_log_ms >= 20) {
        last_log_ms = now_ms;
        ESP_LOGI(TAG, "Buttons: %04x Hat: %d LX: %4d LY: %4d RX: %4d RY: %4d",
                 s.buttons, s.hat, s.lx, s.ly, s.rx, s.ry);
    }
}


// =============================================================================
//  GATT discovery — CCCD write completion
// =============================================================================

static int on_cccd_write(uint16_t conn_handle,
                         const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr,
                         void *arg)
{
    int idx = (int)(intptr_t)arg;
    if (error->status == 0) {
        g_chrs[idx].cccd_handle = attr->handle;
        ESP_LOGI(TAG, "Subscribed notifications: chr[%d] val=0x%04x cccd=0x%04x",
                 idx, g_chrs[idx].val_handle, attr->handle);
    } else {
        ESP_LOGW(TAG, "CCCD write failed: chr[%d] status=%d", idx, error->status);
    }

    // Chain to the next chr's descriptor discovery
    if (idx + 1 < g_chr_count) {
        begin_dsc_discovery(idx + 1);
    } else {
        ESP_LOGI(TAG, "All Report characteristics subscribed — controller ready");
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        g_state.connected = true;
        xSemaphoreGive(g_state_mutex);
    }
    return 0;
}


// =============================================================================
//  GATT discovery — descriptor discovery per characteristic
// =============================================================================
//
//  NimBLE fires this callback once per descriptor found, then once more with
//  error->status == BLE_HS_EDONE to signal completion.
//  We look for the CCCD (0x2902) and write 0x0001 to enable notifications.
//  We must NOT assume the CCCD is at val_handle + 1.

static int on_dsc_discovered(uint16_t conn_handle,
                              const struct ble_gatt_error *error,
                              uint16_t chr_val_handle,
                              const struct ble_gatt_dsc *dsc,
                              void *arg)
{
    int idx = (int)(intptr_t)arg;

    if (error->status == BLE_HS_EDONE) {
        if (g_chrs[idx].cccd_handle == 0) {
            // No CCCD found for this chr — skip, advance to next
            ESP_LOGW(TAG, "No CCCD for chr[%d] (val=0x%04x) — skipping",
                     idx, g_chrs[idx].val_handle);
            if (idx + 1 < g_chr_count) {
                begin_dsc_discovery(idx + 1);
            }
        }
        // If CCCD was found, on_cccd_write will advance the chain
        return 0;
    }

    if (error->status != 0) {
        ESP_LOGE(TAG, "Dsc discovery error: chr[%d] status=%d", idx, error->status);
        return 0;
    }

    ESP_LOGD(TAG, "  dsc: handle=0x%04x uuid=0x%04x",
             dsc->handle, ble_uuid_u16(&dsc->uuid.u));

    if (ble_uuid_cmp(&dsc->uuid.u, &kUuidCccd.u) == 0) {
        ESP_LOGI(TAG, "Found CCCD: handle=0x%04x for chr[%d]", dsc->handle, idx);

        // Mark handle provisionally so the EDONE branch knows CCCD was found
        g_chrs[idx].cccd_handle = dsc->handle;

        uint8_t val[2] = {0x01, 0x00};   // enable notifications
        int rc = ble_gattc_write_flat(conn_handle,
                                      dsc->handle,
                                      val, sizeof(val),
                                      on_cccd_write,
                                      (void *)(intptr_t)idx);
        if (rc != 0) {
            ESP_LOGE(TAG, "CCCD write submit failed: chr[%d] rc=%d", idx, rc);
            g_chrs[idx].cccd_handle = 0;   // reset — write didn't go through
        }
    }
    return 0;
}

static void begin_dsc_discovery(int idx)
{
    if (idx >= g_chr_count) return;

    uint16_t start = g_chrs[idx].val_handle + 1;
    uint16_t end   = g_chrs[idx].end_handle;

    if (start > end) {
        // Degenerate range — no descriptors possible; move on
        ESP_LOGW(TAG, "Empty dsc range for chr[%d] — skipping", idx);
        if (idx + 1 < g_chr_count) begin_dsc_discovery(idx + 1);
        return;
    }

    ESP_LOGI(TAG, "Discovering dscs: chr[%d] val=0x%04x range 0x%04x-0x%04x",
             idx, g_chrs[idx].val_handle, start, end);

    int rc = ble_gattc_disc_all_dscs(g_conn_handle,
                                     start,
                                     end,
                                     on_dsc_discovered,
                                     (void *)(intptr_t)idx);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gattc_disc_all_dscs failed: chr[%d] rc=%d", idx, rc);
        if (idx + 1 < g_chr_count) begin_dsc_discovery(idx + 1);
    }
}


// =============================================================================
//  GATT discovery — characteristic discovery
// =============================================================================
//
//  Collects every 0x2A4D (Report) characteristic within the HID service.
//  Tracks val_handle and, crucially, sets each chr's end_handle to just before
//  the next chr's definition handle — that range is where its descriptors live.
//  After BLE_HS_EDONE kicks off descriptor discovery for all collected chrs.

static int on_chr_discovered(uint16_t conn_handle,
                              const struct ble_gatt_error *error,
                              const struct ble_gatt_chr *chr,
                              void *arg)
{
    if (error->status == BLE_HS_EDONE) {
        ESP_LOGI(TAG, "Chr discovery done — %d Report chr(s) found", g_chr_count);
        if (g_chr_count > 0) {
            begin_dsc_discovery(0);
        } else {
            ESP_LOGE(TAG, "No Report characteristics in HID service");
        }
        return 0;
    }

    if (error->status != 0) {
        ESP_LOGE(TAG, "Chr discovery error: %d", error->status);
        return 0;
    }

    // Walk all characteristics; only keep 0x2A4D (Report) ones
    if (ble_uuid_cmp(&chr->uuid.u, &kUuidReport.u) == 0) {
        if (g_chr_count < MAX_REPORT_CHRS) {
            int i = g_chr_count++;
            g_chrs[i].val_handle  = chr->val_handle;
            g_chrs[i].cccd_handle = 0;
            // end_handle for chr[i] is the service end; corrected below when
            // chr[i+1] arrives, since descriptors live between consecutive chrs.
            g_chrs[i].end_handle  = g_svc_end;

            // Correct the previous chr's end_handle now that we see the next one
            if (i > 0) {
                // Descriptors for chr[i-1] live up to (but not including) chr[i]'s def_handle
                g_chrs[i - 1].end_handle = chr->def_handle - 1;
            }

            ESP_LOGI(TAG, "Report chr[%d]: def=0x%04x val=0x%04x",
                     i, chr->def_handle, chr->val_handle);
        } else {
            ESP_LOGW(TAG, "MAX_REPORT_CHRS exceeded — ignoring additional chrs");
        }
    }
    return 0;
}


// =============================================================================
//  GATT discovery — service discovery
// =============================================================================

static int on_svc_discovered(uint16_t conn_handle,
                              const struct ble_gatt_error *error,
                              const struct ble_gatt_svc *svc,
                              void *arg)
{
    if (error->status == BLE_HS_EDONE) {
        return 0;
    }
    if (error->status != 0) {
        ESP_LOGE(TAG, "Service discovery error: %d", error->status);
        return 0;
    }

    ESP_LOGI(TAG, "HID service found: start=0x%04x end=0x%04x",
             svc->start_handle, svc->end_handle);

    g_chr_count = 0;
    g_svc_end   = svc->end_handle;

    ble_gattc_disc_all_chrs(conn_handle,
                            svc->start_handle,
                            svc->end_handle,
                            on_chr_discovered,
                            NULL);
    return 0;
}

static void start_gatt_discovery(void)
{
    ESP_LOGI(TAG, "Starting GATT discovery (HID service 0x1812)...");
    g_chr_count = 0;
    int rc = ble_gattc_disc_svc_by_uuid(g_conn_handle,
                                        &kUuidHidSvc.u,
                                        on_svc_discovered,
                                        NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gattc_disc_svc_by_uuid failed: %d", rc);
    }
}


// =============================================================================
//  Scanning
// =============================================================================

static bool adv_fields_have_hid_uuid(const struct ble_hs_adv_fields *f)
{
    // NimBLE populates uuids16[] from both Complete and Incomplete UUID16 AD types
    for (int i = 0; i < f->num_uuids16; i++) {
        if (ble_uuid_u16(&f->uuids16[i].u) == 0x1812) return true;
    }
    return false;
}

static void start_scan(void)
{
    // Active scan: the controller puts its full name in the scan response
    struct ble_gap_disc_params p = {};
    p.itvl              = (SCAN_ITVL_MS * 1000) / 625;  // units of 0.625 ms
    p.window            = (SCAN_WIN_MS  * 1000) / 625;
    p.filter_duplicates = 1;
    p.passive           = 0;   // active — request scan responses for full name

    int rc = ble_gap_disc(g_own_addr_type, BLE_HS_FOREVER, &p, gap_event_cb, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "ble_gap_disc failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "Scanning for \"%s\" with HID service...", TARGET_NAME);
    }
}


// =============================================================================
//  GAP event handler
// =============================================================================

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    // ── Advertising / scan-response report ────────────────────────────────────
    case BLE_GAP_EVENT_DISC: {
        const struct ble_gap_disc_desc *disc = &event->disc;
        if (disc->length_data == 0) break;

        struct ble_hs_adv_fields f = {};
        if (ble_hs_adv_parse_fields(&f, disc->data, disc->length_data) != 0) break;

        // Match on name substring (controller name lives in scan response)
        bool name_ok = false;
        if (f.name && f.name_len > 0) {
            // NimBLE does not null-terminate f.name; copy to stack buffer first
            char name_buf[32] = {};
            uint8_t copy_len = (f.name_len < (uint8_t)sizeof(name_buf) - 1)
                               ? f.name_len : (uint8_t)sizeof(name_buf) - 1;
            memcpy(name_buf, f.name, copy_len);
            name_ok = (strstr(name_buf, TARGET_NAME) != NULL);
        }

        // Also accept if it advertises HID UUID AND name field is absent
        // (some controllers only include name in scan response, not in advert)
        bool uuid_ok = adv_fields_have_hid_uuid(&f);

        if (!name_ok && !uuid_ok) break;
        if (!name_ok) break;   // require name match to avoid false positives

        char addr_str[18];
        snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 disc->addr.val[5], disc->addr.val[4], disc->addr.val[3],
                 disc->addr.val[2], disc->addr.val[1], disc->addr.val[0]);
        ESP_LOGI(TAG, "Target found: %s [%s] — stopping scan and connecting",
                 TARGET_NAME, addr_str);

        ble_gap_disc_cancel();

        // Preferred connection parameters for HID devices
        struct ble_gap_conn_params cp = {};
        cp.itvl_min            = 6;     // 7.5 ms
        cp.itvl_max            = 12;    // 15 ms
        cp.latency             = 0;
        cp.supervision_timeout = 400;   // 4 s  (units of 10 ms)
        cp.min_ce_len          = BLE_GAP_INITIAL_CONN_MIN_CE_LEN;
        cp.max_ce_len          = BLE_GAP_INITIAL_CONN_MAX_CE_LEN;

        int rc = ble_gap_connect(g_own_addr_type,
                                 &disc->addr,
                                 CONN_TIMEOUT_MS,
                                 &cp,
                                 gap_event_cb,
                                 NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "ble_gap_connect failed: %d — resuming scan", rc);
            start_scan();
        }
        break;
    }

    // ── Connection established ────────────────────────────────────────────────
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            g_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Connected (handle=%d) — initiating security", g_conn_handle);

            // Kick off pairing or key-exchange resumption for a bonded device.
            // GATT discovery must wait until the link is encrypted; see ENC_CHANGE.
            int rc = ble_gap_security_initiate(g_conn_handle);
            if (rc != 0 && rc != BLE_HS_EALREADY) {
                // Some HID devices connect un-encrypted; try discovery anyway
                ESP_LOGW(TAG, "security_initiate: %d — attempting discovery anyway", rc);
                start_gatt_discovery();
            }
        } else {
            ESP_LOGW(TAG, "Connection attempt failed (status=%d) — rescanning",
                     event->connect.status);
            start_scan();
        }
        break;

    // ── Disconnected ──────────────────────────────────────────────────────────
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected (reason=0x%02x) — rescanning",
                 event->disconnect.reason);
        g_conn_handle = BLE_HS_CONN_HANDLE_NONE;

        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        g_state.connected = false;
        xSemaphoreGive(g_state_mutex);

        start_scan();
        break;

    // ── Encryption / security established ────────────────────────────────────
    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "ENC_CHANGE: conn=%d status=%d",
                 event->enc_change.conn_handle,
                 event->enc_change.status);
        if (event->enc_change.status == 0) {
            // Link is now encrypted — safe to discover GATT services
            start_gatt_discovery();
        } else {
            ESP_LOGE(TAG, "Encryption failed — terminating connection");
            ble_gap_terminate(g_conn_handle, BLE_ERR_AUTH_FAIL);
        }
        break;

    // ── Passkey / numeric comparison (Just Works — no display or keyboard) ────
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        struct ble_sm_io pk = {};
        pk.action = event->passkey.params.action;
        switch (pk.action) {
            case BLE_SM_IOACT_NUMCMP:
                pk.numcmp_accept = 1;  // accept numeric comparison without display
                break;
            case BLE_SM_IOACT_INPUT:
            case BLE_SM_IOACT_DISP:
                pk.passkey = 0;        // Just Works
                break;
            default:
                break;
        }
        ble_sm_inject_io(event->passkey.conn_handle, &pk);
        break;
    }

    // ── Peer identity resolved via IRK ────────────────────────────────────────
    case BLE_GAP_EVENT_IDENTITY_RESOLVED:
        ESP_LOGI(TAG, "Peer identity resolved (IRK)");
        break;

    // ── Controller lost our bond record and wants to re-pair ─────────────────
    // Delete the stale bond entry and allow pairing to continue from scratch.
    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        struct ble_gap_conn_desc desc;
        int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        if (rc == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
            ESP_LOGW(TAG, "Repeat pairing — stale bond deleted; retrying");
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    // ── Subscription state change (informational) ─────────────────────────────
    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGD(TAG, "Subscribe: attr=0x%04x notify=%d",
                 event->subscribe.attr_handle,
                 event->subscribe.cur_notify);
        break;

    // ── Incoming HID notification ─────────────────────────────────────────────
    case BLE_GAP_EVENT_NOTIFY_RX: {
        uint16_t attr = event->notify_rx.attr_handle;

        // Accept if attr_handle matches any tracked Report characteristic
        bool is_report = false;
        for (int i = 0; i < g_chr_count; i++) {
            if (g_chrs[i].val_handle == attr) { is_report = true; break; }
        }
        if (!is_report) break;

        uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
        if (len > 32) len = 32;
        uint8_t data[32];
        ble_hs_mbuf_to_flat(event->notify_rx.om, data, len, NULL);
        decode_report(data, len);
        break;
    }

    default:
        break;
    }
    return 0;
}


// =============================================================================
//  NimBLE host lifecycle callbacks
// =============================================================================

static void on_ble_sync(void)
{
    // Determine the best own address type.
    // ble_hs_util_ensure_addr(0) prefers the public address; falls back to random.
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr: %d", rc);
    }

    rc = ble_hs_id_infer_auto(0, &g_own_addr_type);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_hs_id_infer_auto: %d — defaulting to PUBLIC", rc);
        g_own_addr_type = BLE_OWN_ADDR_PUBLIC;
    }

    start_scan();
}

static void on_ble_reset(int reason)
{
    // The NimBLE host was reset (e.g. controller crash).
    // on_ble_sync will be called again automatically after recovery.
    ESP_LOGW(TAG, "NimBLE host reset (reason=%d)", reason);
    g_conn_handle = BLE_HS_CONN_HANDLE_NONE;

    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    g_state.connected = false;
    xSemaphoreGive(g_state_mutex);
}

// FreeRTOS task that drives the NimBLE host
static void bt_host_task(void *pvParam)
{
    nimble_port_run();          // blocks until nimble_port_stop()
    nimble_port_freertos_deinit();
}


// =============================================================================
//  Public API
// =============================================================================

extern "C" bool k10_bt_begin(void)
{
    // NVS is required both for system use and for NimBLE bond persistence.
    // Note: for bonds to survive reboot, also set CONFIG_BT_NIMBLE_NVS_PERSIST=y.
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    g_state_mutex = xSemaphoreCreateMutex();
    if (g_state_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create state mutex");
        return false;
    }

    ESP_ERROR_CHECK(nimble_port_init());

    // ── Security manager (SM) — Just Works pairing + bonding ─────────────────
    //
    // sm_io_cap = NO_INPUT_NO_OUTPUT → forces "Just Works" (no PIN, no display).
    // sm_sc = 1  → LE Secure Connections (stronger than legacy pairing).
    // sm_bonding = 1 → exchange and store long-term keys after pairing.
    // sm_mitm = 0  → MITM protection not required (required for Just Works).
    //
    // Key distribution: distribute both the encryption key (LTK/EDIV/Rand) and
    // the identity key (IRK + identity address).  The IRK is needed so the ESP32
    // can recognise a reconnecting controller that uses a resolvable private address.
    ble_hs_cfg.sm_io_cap         = BLE_HS_IO_NO_INPUT_OUTPUT;   // Just Works
    ble_hs_cfg.sm_bonding        = 1;
    ble_hs_cfg.sm_mitm           = 0;
    ble_hs_cfg.sm_sc             = 1;
    ble_hs_cfg.sm_our_key_dist   = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    // Initialise the NVS-backed bond store so keys survive reboot.
    // This must be called after nimble_port_init() and before the host task starts.
    ble_store_config_init();

    ble_hs_cfg.sync_cb  = on_ble_sync;
    ble_hs_cfg.reset_cb = on_ble_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Start the NimBLE host on its own FreeRTOS task (pinned to core 0 per sdkconfig)
    nimble_port_freertos_init(bt_host_task);

    ESP_LOGI(TAG, "BLE HID Host started — waiting for %s", TARGET_NAME);
    return true;
}

extern "C" void k10_bt_get_gamepad_state(k10_gamepad_state_t *state)
{
    if (state == NULL) return;
    xSemaphoreTake(g_state_mutex, portMAX_DELAY);
    memcpy(state, &g_state, sizeof(k10_gamepad_state_t));
    xSemaphoreGive(g_state_mutex);
}
