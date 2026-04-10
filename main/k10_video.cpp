#include "k10_video.h"

#include <algorithm>
#include <esp_heap_caps.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "arcade_core/config.h"
#include "arcade_core/pacman_logo.h"
#include "arcade_core/galaga_logo.h"
#include "arcade_core/dkong_logo.h"
#include "arcade_core/frogger_logo.h"
#include "arcade_core/digdug_logo.h"
#include "arcade_core/1942_logo.h"

#include "k10_config.h"
#include "k10_hardware.h"
#include "k10_idf.h"

namespace {

// ── Display geometry ──────────────────────────────────────────────────────────

// All logos are exactly this many pixels tall.
constexpr int kLogoHeight    = 96;
// Tile rows per logo (logo height / tile height).
constexpr int kLogoRowsCount = kLogoHeight / 8;
// Vertical pixel offset so the centred logo is visually middle-of-screen.
constexpr int16_t kLogoTop = (K10_TFT_ACTIVE_HEIGHT - kLogoHeight) / 2;

constexpr uint16_t kPanelBackground = 0x0000;

// ── SPI / TFT state ───────────────────────────────────────────────────────────

spi_device_handle_t g_tft_handle     = nullptr;
spi_transaction_t   g_transactions[2] = {};
bool                g_video_ready    = false;
uint16_t*           g_frame_buffers[2] = {nullptr, nullptr};
uint8_t             g_buffer_index   = 0;
int                 g_in_flight_count = 0;
bool                g_dma_active     = false;

// ── Menu entry table ──────────────────────────────────────────────────────────

struct MenuEntry {
    K10Machine      machine;
    const char*     name;
    const uint16_t* logo;
    uint16_t        accent;
};

const MenuEntry g_menu_entries[] = {
#ifdef ENABLE_PACMAN
    {K10_MACHINE_PACMAN,  "Pac-Man",     pacman_logo,  0x07e0},
#endif
#ifdef ENABLE_GALAGA
    {K10_MACHINE_GALAGA,  "Galaga",      galaga_logo,  0xf800},
#endif
#ifdef ENABLE_DKONG
    {K10_MACHINE_DKONG,   "Donkey Kong", dkong_logo,   0xfd20},
#endif
#ifdef ENABLE_FROGGER
    {K10_MACHINE_FROGGER, "Frogger",     frogger_logo, 0x07ff},
#endif
#ifdef ENABLE_DIGDUG
    {K10_MACHINE_DIGDUG,  "Dig Dug",     digdug_logo,  0xf81f},
#endif
#ifdef ENABLE_1942
    {K10_MACHINE_1942,    "1942",        _1942_logo,   0xffe0},
#endif
};

constexpr size_t kMenuEntryCount = sizeof(g_menu_entries) / sizeof(g_menu_entries[0]);

// ── Logo row cache ────────────────────────────────────────────────────────────
//
// Scanning an RLE-compressed logo from the start every time render_logo() is
// called for a new tile row is O(logo_size) per row.  Instead we precompute
// the decoder state at the start of each 8-pixel logo row once at init, making
// every subsequent seek O(1).

struct LogoRowEntry {
    const uint16_t* ptr;         // data pointer at the start of this row
    uint16_t        carry_color; // color of any RLE run that straddles the row boundary
    uint16_t        carry_count; // how many pixels of carry_color precede fresh data
};

static LogoRowEntry g_logo_cache[kMenuEntryCount][kLogoRowsCount];

// Build the per-row cache for every enabled logo.  Called once during init.
static void build_logo_cache() {
    for (size_t li = 0; li < kMenuEntryCount; ++li) {
        const uint16_t* logo = g_menu_entries[li].logo;
        if (!logo) continue;

        const uint16_t  marker      = logo[0];
        const uint16_t* ptr         = logo + 1;
        uint16_t        carry_color = 0;
        uint16_t        carry_count = 0;

        for (int row = 0; row < kLogoRowsCount; ++row) {
            g_logo_cache[li][row] = {ptr, carry_color, carry_count};

            // Walk through exactly 8 pixel rows worth of pixels (one tile row).
            // Each cache entry spans logo_y values 0, 8, 16, …, 88, so the
            // stride is 8 × K10_TFT_ACTIVE_WIDTH, not just K10_TFT_ACTIVE_WIDTH.
            uint32_t remaining = K10_TFT_ACTIVE_WIDTH * 8;

            // Consume any carry-over from the previous row first.
            if (carry_count > 0) {
                uint32_t take = std::min<uint32_t>(carry_count, remaining);
                carry_count -= static_cast<uint16_t>(take);
                remaining   -= take;
            }

            while (remaining > 0) {
                if (*ptr == marker) {
                    uint32_t run = static_cast<uint32_t>(ptr[1]) + 1;
                    carry_color  = ptr[2];
                    ptr += 3;
                    if (run <= remaining) {
                        remaining  -= run;
                        carry_count = 0;
                    } else {
                        carry_count = static_cast<uint16_t>(run - remaining);
                        remaining   = 0;
                    }
                } else {
                    ++ptr;
                    --remaining;
                }
            }
        }
    }
}

// ── Colour helpers ────────────────────────────────────────────────────────────

// Convert a byte-swapped RGB565 pixel to greyscale.
//
// Logo pixels are stored byte-swapped for direct DMA to the ILI9341 (which
// expects big-endian RGB565).  In this layout a uint16_t value has:
//   bits 15-13 = G[2:0], bits 12-8 = B[4:0], bits 7-3 = R[4:0], bits 2-0 = G[5:3]
//
// The luminance is (2R + G + 2B) / 4 where R,B are 5-bit and G is 6-bit.
// The result is packed back into byte-swapped RGB565 with R = B = luma/2,
// G = luma (green has twice the bit-depth so it carries the full luma value).
static uint16_t greyscale(uint16_t input) {
    const uint16_t r5    = (input >> 3) & 0x1F;
    const uint16_t g6    = ((input << 3) & 0x38) | ((input >> 13) & 0x07);
    const uint16_t b5    = (input >> 8) & 0x1F;
    const uint16_t luma  = static_cast<uint16_t>((2 * r5 + g6 + 2 * b5) / 4);

    // Reconstruct byte-swapped RGB565: R = B = luma[4:0] (via luma[5:1]),
    // G[5:0] = luma[5:0].
    return static_cast<uint16_t>(
        ((luma << 13) & 0xe000) |   // G[2:0]
        ((luma <<  7) & 0x1f00) |   // B[4:0]
        ((luma <<  2) & 0x00f8) |   // R[4:0]
        ((luma >>  3) & 0x0007)     // G[5:3]
    );
}

// ── Logo renderer ─────────────────────────────────────────────────────────────

// Render one 8-pixel tile strip of a logo into tile_buf.
//
//   logo_idx  index into g_menu_entries (and g_logo_cache)
//   logo_y    the logo pixel row that maps to the TOP of this tile; must be
//             a multiple of 8.
//               >= 0  we start at logo_y pixels into the logo
//               <  0  the logo starts |logo_y| pixels below the tile top;
//                     the leading rows of the buffer are left untouched (black)
//   active    true = full colour, false = greyscale
//   tile_buf  caller-provided buffer for K10_TFT_ACTIVE_WIDTH × 8 pixels
static void render_logo(int logo_idx, int16_t logo_y, bool active, uint16_t* tile_buf) {
    const int tile_pixels = K10_TFT_ACTIVE_WIDTH * 8;

    // Buffer range to fill.
    const int buf_start = (logo_y < 0) ? (-logo_y * K10_TFT_ACTIVE_WIDTH) : 0;
    const int logo_px_start = (logo_y >= 0) ? (logo_y * K10_TFT_ACTIVE_WIDTH) : 0;
    const int logo_px_remaining = kLogoHeight * K10_TFT_ACTIVE_WIDTH - logo_px_start;
    const int buf_end = buf_start + std::min(logo_px_remaining, tile_pixels - buf_start);

    if (buf_end <= buf_start) return;

    // Look up the precomputed decoder state for this logo row.
    const int cache_row = (logo_y >= 0) ? (logo_y / 8) : 0;
    const LogoRowEntry& e = g_logo_cache[logo_idx][cache_row];

    const uint16_t  marker = g_menu_entries[logo_idx].logo[0];
    const uint16_t* ptr    = e.ptr;
    uint16_t carry_count   = e.carry_count;
    uint16_t carry_color   = e.carry_color;

    int i = buf_start;

    // Emit any pixels carried over from the RLE run that ended the previous row.
    if (carry_count > 0) {
        const uint16_t c = active ? carry_color : greyscale(carry_color);
        while (carry_count > 0 && i < buf_end) {
            tile_buf[i++] = c;
            --carry_count;
        }
    }

    // Decode and emit pixels until we've filled the buffer range.
    while (i < buf_end) {
        if (*ptr == marker) {
            const uint16_t color = active ? ptr[2] : greyscale(ptr[2]);
            uint16_t run = ptr[1] + 1;
            ptr += 3;
            while (run-- > 0 && i < buf_end) {
                tile_buf[i++] = color;
            }
        } else {
            tile_buf[i++] = active ? *ptr : greyscale(*ptr);
            ++ptr;
        }
    }
}

// ── Menu / machine tile renderers ─────────────────────────────────────────────

static int normalize_selection(int idx) {
    if (idx < 0 || idx >= static_cast<int>(kMenuEntryCount)) return 0;
    return idx;
}

static const MenuEntry* menu_entry_for_machine(int machine) {
    for (size_t i = 0; i < kMenuEntryCount; ++i) {
        if (g_menu_entries[i].machine == static_cast<K10Machine>(machine))
            return &g_menu_entries[i];
    }
    return nullptr;
}

// Returns the index into g_menu_entries for a given machine, or -1.
static int logo_idx_for_machine(int machine) {
    for (size_t i = 0; i < kMenuEntryCount; ++i) {
        if (g_menu_entries[i].machine == static_cast<K10Machine>(machine))
            return static_cast<int>(i);
    }
    return -1;
}

// Render one 8-pixel tile row of the scrolling logo carousel.
//
// The carousel wraps all enabled game logos (each kLogoHeight px tall) in a
// vertical strip that fills the display.  The selected game is rendered in full
// colour; all others are greyscale.  A carousel offset is applied so the game
// two positions before the selection aligns with the top of the screen — this
// places the selected logo in a consistent visual slot regardless of count.
static void render_menu_row(uint16_t* tile_buf, int tile_row, int selection_index) {
    memset(tile_buf, 0, K10_TFT_ACTIVE_WIDTH * 8 * sizeof(uint16_t));

    const int sel   = normalize_selection(selection_index);
    const int count = static_cast<int>(kMenuEntryCount);

    // Shift the carousel so `sel` sits two logo-heights down from the scroll
    // origin, which centres it on a three-logo display.
    const int carousel_offset = kLogoHeight * ((sel + count - 2) % count);
    const int scroll_y        = tile_row * 8 + carousel_offset;

    // Which logo occupies the top of this tile, and how far into it are we?
    // logo_y is always a multiple of 8 because tile_row*8 and carousel_offset
    // are both multiples of 8.
    int     logo_idx = (scroll_y / kLogoHeight) % count;
    int16_t logo_y   = static_cast<int16_t>(scroll_y % kLogoHeight);

    if (g_menu_entries[logo_idx].logo) {
        render_logo(logo_idx, logo_y, sel == logo_idx, tile_buf);
    }

    // If this logo ends mid-tile, render the leading rows of the next logo.
    if (logo_y > kLogoHeight - 8) {
        const int next_idx = (logo_idx + 1) % count;
        // next_y is negative: the next logo begins |next_y| pixels below tile top.
        const int16_t next_y = static_cast<int16_t>(logo_y - kLogoHeight);
        if (g_menu_entries[next_idx].logo) {
            render_logo(next_idx, next_y, sel == next_idx, tile_buf);
        }
    }
}

static void fill_row(uint16_t* buffer, uint16_t color) {
    for (uint32_t i = 0; i < K10_TFT_ACTIVE_WIDTH * 8; ++i) {
        buffer[i] = color;
    }
}

// Render one 8-pixel tile row of the "you are playing" splash screen shown
// while a game is starting.  Draws the game logo centred on a black background
// with accent-colour bars above, below, and flanking the logo centre.
static void render_machine_row(uint16_t* tile_buf, uint16_t tile_row, int machine) {
    memset(tile_buf, 0, K10_TFT_ACTIVE_WIDTH * 8 * sizeof(uint16_t));

    const MenuEntry* entry  = menu_entry_for_machine(machine);
    const uint16_t   accent = entry ? entry->accent : 0xffff;

    // Solid accent bars at the top and bottom two tile rows.
    const int last_row = K10_TFT_ACTIVE_HEIGHT / 8 - 1;
    if (tile_row <= 1 || tile_row >= last_row - 1) {
        fill_row(tile_buf, accent);
        return;
    }

    // Narrower accent bar flanking the logo centre (96 px wide, horizontally centred).
    constexpr uint32_t bar_start = (K10_TFT_ACTIVE_WIDTH / 2) - 48;
    constexpr uint32_t bar_end   = (K10_TFT_ACTIVE_WIDTH / 2) + 48;
    if (tile_row == 4 || tile_row == last_row - 4) {
        for (uint32_t i = 0; i < K10_TFT_ACTIVE_WIDTH * 8; ++i) {
            const uint32_t x = i % K10_TFT_ACTIVE_WIDTH;
            if (x >= bar_start && x < bar_end) {
                tile_buf[i] = accent;
            }
        }
    }

    // Render the centred game logo.
    const int logo_idx = logo_idx_for_machine(machine);
    const int16_t logo_y = static_cast<int16_t>(tile_row * 8 - kLogoTop);
    if (logo_idx >= 0 && entry && entry->logo && logo_y > -8 && logo_y < kLogoHeight) {
        render_logo(logo_idx, logo_y, true, tile_buf);
    }
}

// ── Strip rendering helper ────────────────────────────────────────────────────
//
// Both draw functions share the same DMA strip loop.  This template avoids
// code duplication without the overhead of a virtual call.
template<typename RowRenderer>
static void draw_frame_strips(RowRenderer render_tile_row) {
    k10_video_begin_frame();
    for (int strip = 0; strip < K10_TFT_STRIP_COUNT; ++strip) {
        uint16_t* buf = k10_video_get_draw_buffer();
        for (int i = 0; i < K10_TFT_STRIP_HEIGHT / 8; ++i) {
            const int tile_row = strip * (K10_TFT_STRIP_HEIGHT / 8) + i;
            render_tile_row(buf + K10_TFT_ACTIVE_WIDTH * 8 * i, tile_row);
        }
        k10_video_write(buf, K10_TFT_ACTIVE_WIDTH * K10_TFT_STRIP_HEIGHT);
    }
    k10_video_end_frame();
}

// ── SPI / TFT low-level ───────────────────────────────────────────────────────

void configure_output_gpio(int pin, uint32_t initial_level) {
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
    gpio_set_level(static_cast<gpio_num_t>(pin), initial_level);
}

void write_gpio(int pin, uint32_t level) {
    gpio_set_level(static_cast<gpio_num_t>(pin), level);
}

spi_device_interface_config_t g_if_cfg = {
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .mode = 0,
    .duty_cycle_pos = 128,
    .cs_ena_pretrans = 0,
    .cs_ena_posttrans = 0,
    .clock_speed_hz = K10_TFT_SPICLK,
    .input_delay_ns = 0,
    .spics_io_num = -1,
    .flags = SPI_DEVICE_HALFDUPLEX,
    .queue_size = 2,
    .pre_cb = nullptr,
    .post_cb = nullptr,
};

spi_bus_config_t g_bus_cfg = {
    .mosi_io_num = K10_TFT_MOSI,
    .miso_io_num = K10_TFT_MISO,
    .sclk_io_num = K10_TFT_SCLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .data4_io_num = -1,
    .data5_io_num = -1,
    .data6_io_num = -1,
    .data7_io_num = -1,
    .max_transfer_sz = K10_TFT_WIDTH * K10_TFT_STRIP_HEIGHT * 2,
    .flags = SPICOMMON_BUSFLAG_MASTER,
    .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
    .intr_flags = 0,
};

#define W16(a) ((a) >> 8), ((a) & 0xff)

const uint8_t kInitCmds[] = {
    0xEF, 3, 0x03, 0x80, 0x02,
    0xCF, 3, 0x00, 0xC1, 0x30,
    0xED, 4, 0x64, 0x03, 0x12, 0x81,
    0xE8, 3, 0x85, 0x00, 0x78,
    0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
    0xF7, 1, 0x20,
    0xEA, 2, 0x00, 0x00,
    0xC0, 1, 0x23,
    0xC1, 1, 0x10,
    0xC5, 2, 0x3e, 0x28,
    0xC7, 1, 0x86,
    0x36, 1, static_cast<uint8_t>(K10_TFT_MADCTL ^ 0xc0),
    0x37, 1, 0x00,
    0x3A, 1, 0x55,
    0xB1, 2, 0x00, 0x18,
    0xB6, 3, 0x08, 0x82, 0x27,
    0xF2, 1, 0x00,
    0x26, 1, 0x01,
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
    0x11, 0,
    0xff, 150,
    0x29, 0,
    0xff, 150,
    0x00,
};

void write_polling(const uint8_t* data, size_t len) {
    if (len == 0) return;
    spi_transaction_t t = {};
    if (len <= 4) {
        t.flags = SPI_TRANS_USE_TXDATA;
        memcpy(t.tx_data, data, len);
    } else {
        t.tx_buffer = data;
    }
    t.length = static_cast<uint16_t>(len * 8);
    spi_device_polling_transmit(g_tft_handle, &t);
}

void write8(uint8_t value) {
    write_polling(&value, 1);
}

void write16(uint16_t value) {
    const uint8_t data[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xff)};
    write_polling(data, 2);
}

void write_command(uint8_t command) {
    write_gpio(K10_TFT_DC, 0);
    write8(command);
    write_gpio(K10_TFT_DC, 1);
}

void send_command(uint8_t command, const uint8_t* data, uint8_t len) {
    write_gpio(K10_TFT_CS, 0);
    write_command(command);
    write_polling(data, len);
    write_gpio(K10_TFT_CS, 1);
}

void set_addr_window(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    uint8_t data[4];

    write_command(0x2A);
    data[0] = static_cast<uint8_t>(x >> 8);
    data[1] = static_cast<uint8_t>(x & 0xff);
    data[2] = static_cast<uint8_t>((x + width - 1) >> 8);
    data[3] = static_cast<uint8_t>((x + width - 1) & 0xff);
    write_polling(data, 4);

    write_command(0x2B);
    data[0] = static_cast<uint8_t>(y >> 8);
    data[1] = static_cast<uint8_t>(y & 0xff);
    data[2] = static_cast<uint8_t>((y + height - 1) >> 8);
    data[3] = static_cast<uint8_t>((y + height - 1) & 0xff);
    write_polling(data, 4);

    write_command(0x2C);
}

void flush_dma() {
    while (g_in_flight_count > 0) {
        spi_transaction_t* completed = nullptr;
        spi_device_get_trans_result(g_tft_handle, &completed, portMAX_DELAY);
        g_in_flight_count--;
    }
    g_dma_active = false;
}

void clear_panel(uint16_t color) {
    flush_dma();
    write_gpio(K10_TFT_CS, 0);
    set_addr_window(0, 0, K10_TFT_WIDTH, K10_TFT_HEIGHT);

    uint16_t* buffer = g_frame_buffers[0];
    if (buffer != nullptr) {
        const uint16_t swapped = static_cast<uint16_t>((color >> 8) | (color << 8));
        for (int i = 0; i < K10_TFT_WIDTH; ++i) {
            buffer[i] = swapped;
        }
        for (int row = 0; row < K10_TFT_HEIGHT; ++row) {
            k10_video_write(buffer, K10_TFT_WIDTH);
        }
        flush_dma();
    } else {
        for (uint32_t i = 0; i < static_cast<uint32_t>(K10_TFT_WIDTH) * K10_TFT_HEIGHT; ++i) {
            write16(color);
        }
    }
    write_gpio(K10_TFT_CS, 1);
}

}  // namespace

// ── Public API ────────────────────────────────────────────────────────────────

bool k10_video_begin() {
    if (g_video_ready) return true;

    configure_output_gpio(K10_TFT_CS, 1);
    configure_output_gpio(K10_TFT_DC, 1);
    configure_output_gpio(K10_TFT_ENABLE, 1);

    esp_err_t result = spi_bus_initialize(K10_TFT_SPI_HOST, &g_bus_cfg, SPI_DMA_CH_AUTO);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        printf("Video: SPI bus init failed: 0x%x\n", result);
        return false;
    }

    result = spi_bus_add_device(K10_TFT_SPI_HOST, &g_if_cfg, &g_tft_handle);
    if (result == ESP_ERR_INVALID_STATE && g_tft_handle != nullptr) {
        result = ESP_OK;
    }
    if (result != ESP_OK) {
        printf("Video: SPI device add failed: 0x%x\n", result);
        return false;
    }

    send_command(0x01, nullptr, 0);
    k10_delay_ms(150);

    const uint8_t* cursor = kInitCmds;
    while (*cursor != 0x00) {
        const uint8_t command = *cursor++;
        const uint8_t len     = *cursor++;
        if (command == 0xff) {
            k10_delay_ms(len);
            continue;
        }
        send_command(command, cursor, len);
        cursor += len;
    }

    g_frame_buffers[0] = static_cast<uint16_t*>(heap_caps_malloc(
        K10_TFT_ACTIVE_WIDTH * K10_TFT_STRIP_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA));
    g_frame_buffers[1] = static_cast<uint16_t*>(heap_caps_malloc(
        K10_TFT_ACTIVE_WIDTH * K10_TFT_STRIP_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA));
    if (!g_frame_buffers[0] || !g_frame_buffers[1]) {
        printf("Video: frame buffer allocation failed\n");
        return false;
    }

    clear_panel(kPanelBackground);

    if (!k10_prepare_expander()) {
        printf("Video: expander prepare failed\n");
    }

    build_logo_cache();

    g_video_ready = true;
    printf("Video: ILI9341 initialized\n");
    return true;
}

void k10_video_begin_frame() {
    if (!g_video_ready && !k10_video_begin()) return;
    flush_dma();
    write_gpio(K10_TFT_CS, 0);
    set_addr_window(K10_TFT_X_OFFSET, K10_TFT_Y_OFFSET, K10_TFT_ACTIVE_WIDTH, K10_TFT_ACTIVE_HEIGHT);
}

void k10_video_write(const uint16_t* colors, uint32_t len) {
    if (!g_video_ready) return;

    int trans_idx = g_buffer_index;
    memset(&g_transactions[trans_idx], 0, sizeof(spi_transaction_t));
    g_transactions[trans_idx].length    = len * 16;
    g_transactions[trans_idx].tx_buffer = colors;

    spi_device_queue_trans(g_tft_handle, &g_transactions[trans_idx], portMAX_DELAY);
    g_in_flight_count++;
    g_dma_active = true;

    g_buffer_index = static_cast<uint8_t>(1 - g_buffer_index);
}

void k10_video_end_frame() {
    if (!g_video_ready) return;
    flush_dma();
    write_gpio(K10_TFT_CS, 1);
}

void k10_video_draw_menu_frame(int selection_index) {
    if (!g_video_ready && !k10_video_begin()) return;
    draw_frame_strips([selection_index](uint16_t* buf, int tile_row) {
        render_menu_row(buf, tile_row, selection_index);
    });
}

void k10_video_draw_machine_frame(int machine) {
    if (!g_video_ready && !k10_video_begin()) return;
    draw_frame_strips([machine](uint16_t* buf, int tile_row) {
        render_machine_row(buf, static_cast<uint16_t>(tile_row), machine);
    });
}

uint16_t* k10_video_get_draw_buffer() {
    if (g_in_flight_count >= 2) {
        spi_transaction_t* completed = nullptr;
        spi_device_get_trans_result(g_tft_handle, &completed, portMAX_DELAY);
        g_in_flight_count--;
    }
    return g_frame_buffers[g_buffer_index];
}

int k10_video_menu_count() {
    return static_cast<int>(kMenuEntryCount);
}

int k10_video_wrap_menu_selection(int selection_index, int delta) {
    if (kMenuEntryCount == 0) return 0;
    const int count = static_cast<int>(kMenuEntryCount);
    // The double-modulo handles negative delta without a loop.
    return ((selection_index + delta) % count + count) % count;
}

const char* k10_video_menu_name(int selection_index) {
    if (kMenuEntryCount == 0) return "Empty";
    const int idx = normalize_selection(selection_index);
    return g_menu_entries[idx].name;
}

K10Machine k10_video_menu_machine(int selection_index) {
    if (kMenuEntryCount == 0) return K10_MACHINE_MENU;
    const int idx = normalize_selection(selection_index);
    return g_menu_entries[idx].machine;
}
