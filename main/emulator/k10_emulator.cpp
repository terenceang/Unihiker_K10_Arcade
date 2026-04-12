#include "k10_emulator.h"

#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/idf_additions.h"

#include "k10_hardware.h"
#include "k10_idf.h"
#include "k10_video.h"

extern "C" {
#include "arcade_core/config.h"
#define IO_EMULATION
#include "arcade_core/emulation.h"

struct Z80;
}

#include "config/k10_game_registry.h"

struct sprite_S {
    unsigned char code;
    unsigned char color;
    unsigned char flags;
    short x;
    short y;
};

void leds_state_reset() {}
void leds_check_galaga_sprite(struct sprite_S* spr) {
    (void)spr;
}

unsigned char active_sprites = 0;
struct sprite_S* sprite = nullptr;
unsigned short* frame_buffer = nullptr;

#include "arcade_core/tileaddr.h"
#ifdef ENABLE_GALAGA
#include "arcade_core/games/galaga/galaga.h"
#endif
#ifdef ENABLE_PACMAN
#include "arcade_core/games/pacman/pacman.h"
#endif
#ifdef ENABLE_DKONG
#include "arcade_core/games/dkong/dkong.h"
#endif
#ifdef ENABLE_1942
#include "arcade_core/games/_1942/1942.h"
#endif
#ifdef ENABLE_FROGGER
#include "arcade_core/games/frogger/frogger.h"
#endif
#ifdef ENABLE_DIGDUG
#include "arcade_core/games/digdug/digdug.h"
#endif

namespace {

constexpr uint32_t kFramePixels = K10_TFT_ACTIVE_WIDTH * K10_TFT_STRIP_HEIGHT;
constexpr uint32_t kFrameBytes = kFramePixels * sizeof(uint16_t);
// The original arcade boards for the supported titles run at about 60 Hz,
// so keep the present loop pinned to that cadence instead of speeding games up.
constexpr uint64_t kFramePeriodUs = 16667;
constexpr uint32_t kEmulationTaskStackWords = 8192;
constexpr UBaseType_t kEmulationTaskPriority = 3;
constexpr BaseType_t kEmulationTaskCore = 0;
constexpr bool kEnableRuntimeProfiling = true;

TaskHandle_t g_emulation_task = nullptr;
TaskHandle_t g_present_task = nullptr;
uint8_t g_buffer_index = 0;
bool g_runtime_ready = false;
bool g_runtime_running = false;
uint8_t g_cached_buttons = 0;

unsigned short snd_boom_cnt = 0;
const signed char* snd_boom_ptr = nullptr;

unsigned long snd_cnt[3] = {0, 0, 0};
unsigned long snd_freq[3] = {0, 0, 0};
const signed char* snd_wave[3] = {nullptr, nullptr, nullptr};
unsigned char snd_volume[3] = {0, 0, 0};

int16_t* snd_buffer = nullptr;

#if defined(ENABLE_FROGGER) || defined(ENABLE_1942)
int ay_period[2][4]    = {{0,0,0,0},{0,0,0,0}};
int ay_volume[2][3]    = {{0,0,0},{0,0,0}};
int ay_enable[2][3]    = {{0,0,0},{0,0,0}};
int audio_cnt[2][4]    = {{0,0,0,0},{0,0,0,0}};
int audio_toggle[2][4] = {{1,1,1,1},{1,1,1,1}};
unsigned long ay_noise_rng[2] = {1, 1};
#endif

IRAM_ATTR int16_t clamp_pcm16(int sample) {
    if (sample > 32767) return 32767;
    if (sample < -32768) return -32768;
    return static_cast<int16_t>(sample);
}

template <typename RenderRowFn>
inline void render_two_rows(RenderRowFn render_fn, short row0, short row1) {
    render_fn(row0);
    frame_buffer += K10_TFT_ACTIVE_WIDTH * 8;
    render_fn(row1);
}

IRAM_ATTR void render_line_pair(short pair_row) {
    const short row0 = pair_row * 2;
    const short row1 = pair_row * 2 + 1;

#ifdef ENABLE_PACMAN
PACMAN_BEGIN
    render_two_rows(pacman_render_row, row0, row1);
PACMAN_END
#endif
#ifdef ENABLE_GALAGA
GALAGA_BEGIN
    render_two_rows(galaga_render_row, row0, row1);
GALAGA_END
#endif
#ifdef ENABLE_DKONG
DKONG_BEGIN
    render_two_rows(dkong_render_row, row0, row1);
DKONG_END
#endif
#ifdef ENABLE_FROGGER
FROGGER_BEGIN
    render_two_rows(frogger_render_row, row0, row1);
FROGGER_END
#endif
#ifdef ENABLE_DIGDUG
DIGDUG_BEGIN
    render_two_rows(digdug_render_row, row0, row1);
DIGDUG_END
#endif
#ifdef ENABLE_1942
_1942_BEGIN
    render_two_rows(_1942_render_row, row0, row1);
_1942_END
#endif

}

IRAM_ATTR void render_line(short strip_row) {
    memset(frame_buffer, 0, kFrameBytes);
    unsigned short* const original_buffer = frame_buffer;
    constexpr int kPairPixels = K10_TFT_ACTIVE_WIDTH * 16;
    constexpr int kPairsPerStrip = K10_TFT_STRIP_HEIGHT / 16;

    for (int pair = 0; pair < kPairsPerStrip; ++pair) {
        frame_buffer = original_buffer + pair * kPairPixels;
        render_line_pair(static_cast<short>(strip_row * kPairsPerStrip + pair));
    }

    frame_buffer = original_buffer;
}

#ifdef ENABLE_GALAGA
void audio_galaga_waveregs_parse() {
    for (int ch = 0; ch < 3; ++ch) {
        snd_volume[ch] = soundregs[ch * 5 + 0x15];
        if (!snd_volume[ch]) continue;

        snd_freq[ch]  = (ch == 0) ? soundregs[0x10] : 0;
        snd_freq[ch] += soundregs[ch * 5 + 0x11] << 4;
        snd_freq[ch] += soundregs[ch * 5 + 0x12] << 8;
        snd_freq[ch] += soundregs[ch * 5 + 0x13] << 12;
        snd_freq[ch] += soundregs[ch * 5 + 0x14] << 16;
        snd_wave[ch]  = galaga_wavetable[soundregs[ch * 5 + 0x05] & 0x07];
    }
}
#endif

#ifdef ENABLE_PACMAN
// Pac-Man Namco WSG register layout (each reg holds one 4-bit nibble).
// Identical freq/volume layout as Galaga (soundregs[ch*5 + 0x10..0x15]).
// Difference: Pac-Man wavetable has 16 entries (4-bit index), Galaga has 8 (3-bit).
void audio_pacman_waveregs_parse() {
    for (int ch = 0; ch < 3; ++ch) {
        snd_volume[ch] = soundregs[ch * 5 + 0x15];
        if (!snd_volume[ch]) continue;

        snd_freq[ch]  = (ch == 0) ? soundregs[0x10] : 0;
        snd_freq[ch] += soundregs[ch * 5 + 0x11] << 4;
        snd_freq[ch] += soundregs[ch * 5 + 0x12] << 8;
        snd_freq[ch] += soundregs[ch * 5 + 0x13] << 12;
        snd_freq[ch] += soundregs[ch * 5 + 0x14] << 16;
        snd_wave[ch]  = pacman_wavetable[soundregs[ch * 5 + 0x05] & 0x0f];
    }
}
#endif

#ifdef ENABLE_DIGDUG
void audio_digdug_waveregs_parse() {
    for (int ch = 0; ch < 3; ++ch) {
        snd_volume[ch] = soundregs[ch * 5 + 0x15];
        if (!snd_volume[ch]) continue;

        snd_freq[ch]  = (ch == 0) ? soundregs[0x10] : 0;
        snd_freq[ch] += soundregs[ch * 5 + 0x11] << 4;
        snd_freq[ch] += soundregs[ch * 5 + 0x12] << 8;
        snd_freq[ch] += soundregs[ch * 5 + 0x13] << 12;
        snd_freq[ch] += soundregs[ch * 5 + 0x14] << 16;
        snd_wave[ch]  = digdug_wavetable[soundregs[ch * 5 + 0x05] & 0x0f];
    }
}
#endif

void audio_namco_waveregs_parse_cpp() {
#ifdef ENABLE_PACMAN
    if (MACHINE_IS_PACMAN) { audio_pacman_waveregs_parse(); return; }
#endif
#ifdef ENABLE_FROGGER
    if (MACHINE_IS_FROGGER) {
        for (int c = 0; c < 3; ++c) {
            ay_period[0][c] = soundregs[2*c] + 256 * (soundregs[2*c + 1] & 15);
            ay_enable[0][c] = (((soundregs[7] >> c) & 1) | ((soundregs[7] >> (c+2)) & 2)) ^ 3;
            ay_volume[0][c] = soundregs[8 + c] & 0x0f;
        }
        ay_period[0][3] = soundregs[6] & 0x1f;
        return;
    }
#endif
#ifdef ENABLE_1942
    if (MACHINE_IS_1942) {
        for (int ay = 0; ay < 2; ++ay) {
            const int ay_off = 16 * ay;
            for (int c = 0; c < 3; ++c) {
                ay_period[ay][c] = soundregs[ay_off + 2*c] + 256 * (soundregs[ay_off + 2*c + 1] & 15);
                ay_enable[ay][c] = (((soundregs[ay_off + 7] >> c) & 1) | ((soundregs[ay_off + 7] >> (c+2)) & 2)) ^ 3;
                ay_volume[ay][c] = soundregs[ay_off + 8 + c] & 0x0f;
            }
            ay_period[ay][3] = soundregs[ay_off + 6] & 0x1f;
        }
        return;
    }
#endif
#ifdef ENABLE_DIGDUG
    if (MACHINE_IS_DIGDUG) { audio_digdug_waveregs_parse(); return; }
#endif
#ifdef ENABLE_GALAGA
    if (MACHINE_IS_GALAGA) { audio_galaga_waveregs_parse(); }
#endif
}

IRAM_ATTR void snd_render_buffer_cpp() {
    const int32_t vol_scale = (64 * AUDIO_VOLUME) / 100;

#ifdef ENABLE_DKONG
    if (MACHINE_IS_DKONG) {
        constexpr int32_t kDkongPcmScale = 256;
        const bool has_queue_data = dkong_audio_rptr != dkong_audio_wptr;
        for (int index = 0; index < K10_AUDIO_BUFFER_FRAMES; ++index) {
            int32_t value = 0;

            if (has_queue_data) {
                // DK audio CPU emits 64-byte chunks. Mirror the legacy path by
                // duplicating each byte to fill the current audio frame size.
                const int source_index = (index >> 1) & 0x3f;
                const int raw = dkong_audio_transfer_buffer[dkong_audio_rptr][source_index];
                value = (raw - 128) * kDkongPcmScale;
            }

            value = (value * AUDIO_VOLUME) / 100;
            const int16_t sample = clamp_pcm16(value);
            snd_buffer[2 * index] = sample;
            snd_buffer[2 * index + 1] = sample;
        }

        if (has_queue_data) {
            dkong_audio_rptr = (dkong_audio_rptr + 1) & DKONG_AUDIO_QUEUE_MASK;
        }
        return;
    }
#endif

#if defined(ENABLE_FROGGER) || defined(ENABLE_1942)
    if (
#ifdef ENABLE_FROGGER
        MACHINE_IS_FROGGER ||
#endif
#ifdef ENABLE_1942
        MACHINE_IS_1942 ||
#endif
        false) {
        const int ay_chip_count =
#ifdef ENABLE_1942
            MACHINE_IS_1942 ? 2 :
#endif
            1;
        const int AY_INC =
    #ifdef ENABLE_FROGGER
            MACHINE_IS_FROGGER ? 9 :
    #endif
            8;
        const int AY_VOL =
    #ifdef ENABLE_FROGGER
            MACHINE_IS_FROGGER ? 11 :
    #endif
            10;
        for (int index = 0; index < K10_AUDIO_BUFFER_FRAMES; ++index) {
            int32_t value = 0;
            for (int ay = 0; ay < ay_chip_count; ++ay) {
                if (ay_period[ay][3]) {
                    audio_cnt[ay][3] += AY_INC;
                    if (audio_cnt[ay][3] > ay_period[ay][3]) {
                        audio_cnt[ay][3] -= ay_period[ay][3];
                        ay_noise_rng[ay] ^= (((ay_noise_rng[ay] & 1) ^ ((ay_noise_rng[ay] >> 3) & 1)) << 17);
                        ay_noise_rng[ay] >>= 1;
                    }
                }
                for (int c = 0; c < 3; ++c) {
                    if (ay_period[ay][c] && ay_volume[ay][c] && ay_enable[ay][c]) {
                        int bit = 1;
                        if (ay_enable[ay][c] & 1) bit &= (audio_toggle[ay][c] > 0) ? 1 : 0;
                        if (ay_enable[ay][c] & 2) bit &= (int)(ay_noise_rng[ay] & 1);
                        if (bit == 0) bit = -1;
                        value += AY_VOL * bit * ay_volume[ay][c];
                        audio_cnt[ay][c] += AY_INC;
                        if (audio_cnt[ay][c] > ay_period[ay][c]) {
                            audio_cnt[ay][c] -= ay_period[ay][c];
                            audio_toggle[ay][c] = -audio_toggle[ay][c];
                        }
                    }
                }
            }
            value *= vol_scale;
            const int16_t sample = clamp_pcm16(value);
            snd_buffer[2 * index] = sample;
            snd_buffer[2 * index + 1] = sample;
        }
        return;
    }
#endif

    for (int index = 0; index < K10_AUDIO_BUFFER_FRAMES; ++index) {
        int32_t value = 0;

        if (snd_volume[0]) value += snd_volume[0] * snd_wave[0][(snd_cnt[0] >> 13) & 0x1f];
        if (snd_volume[1]) value += snd_volume[1] * snd_wave[1][(snd_cnt[1] >> 13) & 0x1f];
        if (snd_volume[2]) value += snd_volume[2] * snd_wave[2][(snd_cnt[2] >> 13) & 0x1f];

        if (snd_boom_cnt) {
            value += *snd_boom_ptr;
            if (snd_boom_cnt & 1) snd_boom_ptr++;
            snd_boom_cnt--;
        }

        value *= vol_scale;

        const int16_t sample = clamp_pcm16(value);
        snd_buffer[2 * index] = sample;
        snd_buffer[2 * index + 1] = sample;

        snd_cnt[0] += snd_freq[0];
        snd_cnt[1] += snd_freq[1];
        snd_cnt[2] += snd_freq[2];
    }
}

void snd_transmit_cpp() {
    if (snd_buffer == nullptr) return;
    
    size_t bytes_out = 0;
    // Synthesis loop: render and transmit as many buffers as I2S can accept
    do {
        bytes_out = k10_audio_write(snd_buffer, K10_AUDIO_BUFFER_BYTES);
        if (bytes_out) {
            audio_namco_waveregs_parse_cpp();
            snd_render_buffer_cpp();
        }
    } while (bytes_out);
}

void update_screen_cpp() {
    static const signed char star_speeds[8] = {-1, -2, -3, 0, 3, 2, 1, 0};
    static uint32_t frame_count = 0;
    static uint32_t last_fps_time = 0;
    static uint64_t next_frame_deadline_us = 0;
    frame_count++;

    if (g_emulation_task != nullptr) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    const uint32_t now = k10_millis();
    if (kEnableRuntimeProfiling && now - last_fps_time >= 5000) {
        const float fps = last_fps_time == 0 ? 0.0f : (frame_count * 1000.0f) / (now - last_fps_time);
        printf("FPS: %.2f, Free Heap: %u\n", fps, esp_get_free_heap_size());
        frame_count = 0;
        last_fps_time = now;
    }

#ifdef ENABLE_PACMAN
PACMAN_BEGIN
    pacman_prepare_frame();
PACMAN_END
#endif
#ifdef ENABLE_GALAGA
GALAGA_BEGIN
    galaga_prepare_frame();
GALAGA_END
#endif
#ifdef ENABLE_DKONG
DKONG_BEGIN
    dkong_prepare_frame();
DKONG_END
#endif
#ifdef ENABLE_FROGGER
FROGGER_BEGIN
    frogger_prepare_frame();
FROGGER_END
#endif
#ifdef ENABLE_DIGDUG
DIGDUG_BEGIN
    digdug_prepare_frame();
DIGDUG_END
#endif
#ifdef ENABLE_1942
_1942_BEGIN
    _1942_prepare_frame();
_1942_END
#endif

    k10_video_begin_frame();
    for (int strip_row = 0; strip_row < K10_TFT_STRIP_COUNT; ++strip_row) {
        frame_buffer = k10_video_get_draw_buffer();
        render_line(static_cast<short>(strip_row));
        k10_video_write(frame_buffer, kFramePixels);
    }
    k10_video_end_frame();

    snd_transmit_cpp();

    if (g_emulation_task != nullptr) {
        xTaskNotifyGive(g_emulation_task);
    }

    const uint64_t now_us = k10_micros();
    if (next_frame_deadline_us == 0 || now_us > next_frame_deadline_us + kFramePeriodUs) {
        next_frame_deadline_us = now_us + kFramePeriodUs;
    } else {
        while (true) {
            const uint64_t current_us = k10_micros();
            if (current_us >= next_frame_deadline_us) {
                break;
            }

            const uint64_t remaining_us = next_frame_deadline_us - current_us;
            if (remaining_us > 2000) {
                vTaskDelay(pdMS_TO_TICKS((remaining_us - 1000) / 1000));
            } else {
                taskYIELD();
            }
        }
        next_frame_deadline_us += kFramePeriodUs;
    }

#ifdef ENABLE_GALAGA
GALAGA_BEGIN
    stars_scroll_y += 2 * star_speeds[starcontrol & 7];
GALAGA_END
#endif
}

void emulation_task(void* parameter) {
    (void)parameter;
    while (true) {
        emulate_frame();
        if (g_present_task != nullptr) {
            xTaskNotifyGive(g_present_task);
        }
    }
}

bool ensure_runtime_allocations() {
    if (sprite == nullptr) {
        sprite = static_cast<sprite_S*>(heap_caps_malloc(128 * sizeof(sprite_S), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
    }
    
    if (snd_buffer == nullptr) {
        snd_buffer = static_cast<int16_t*>(heap_caps_malloc(K10_AUDIO_BUFFER_BYTES, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
    }

    g_runtime_ready = k10_video_get_draw_buffer() != nullptr && sprite != nullptr && snd_buffer != nullptr;
    return g_runtime_ready;
}

void reset_audio_state() {
    memset(snd_cnt, 0, sizeof(snd_cnt));
    memset(snd_freq, 0, sizeof(snd_freq));
    memset(snd_volume, 0, sizeof(snd_volume));
    if (snd_buffer != nullptr) {
        memset(snd_buffer, 0, K10_AUDIO_BUFFER_BYTES);
    }
    snd_boom_cnt = 0;
    snd_boom_ptr = nullptr;
#ifdef ENABLE_DKONG
    dkong_audio_rptr = 0;
    dkong_audio_wptr = 0;
#endif
#if defined(ENABLE_FROGGER) || defined(ENABLE_1942)
    memset(ay_period,  0, sizeof(ay_period));
    memset(ay_volume,  0, sizeof(ay_volume));
    memset(ay_enable,  0, sizeof(ay_enable));
    memset(audio_cnt,  0, sizeof(audio_cnt));
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 4; ++j) audio_toggle[i][j] = 1;
    ay_noise_rng[0] = ay_noise_rng[1] = 1;
#endif
}

void teardown_emulation_task() {
    if (g_emulation_task != nullptr) {
        vTaskDelete(g_emulation_task);
        g_emulation_task = nullptr;
    }

    g_present_task = nullptr;

    if (memory != nullptr) {
        heap_caps_free(memory);
        memory = nullptr;
    }
    
    if (snd_buffer != nullptr) {
        heap_caps_free(snd_buffer);
        snd_buffer = nullptr;
    }

    g_runtime_running = false;
}

}  // namespace

#ifdef ENABLE_GALAGA
extern "C" void galaga_trigger_sound_explosion(void) {
    if (game_started) {
        snd_boom_cnt = 2 * sizeof(galaga_sample_boom);
        snd_boom_ptr = reinterpret_cast<const signed char*>(galaga_sample_boom);
    }
}
#endif

#ifdef ENABLE_DKONG
extern "C" void dkong_trigger_sound(char /*sound_index*/) {
    // DKONG sound is triggered by the emulation core, but the actual audio
    // playback path is handled elsewhere. Keep this symbol defined so the
    // DKONG Z80 emulation can link correctly when DKONG support is enabled.
}
#endif

extern "C" IRAM_ATTR unsigned char buttons_get(void) {
    return g_cached_buttons;
}

extern "C" void audio_dkong_bitrate(char is_dkong) {
    k10_audio_set_dkong_rate(is_dkong != 0);
}

extern "C" unsigned short LoopZ80(Z80* cpu_state) {
    (void)cpu_state;
    return 0xFFFF;
}

bool k10_emulator_supports_machine(K10Machine machine) {
    return k10_machine_is_enabled(machine);
}

bool k10_emulator_start(K10Machine machine) {
    if (!k10_emulator_supports_machine(machine)) {
        return false;
    }

    teardown_emulation_task();

    if (!k10_video_set_machine_clock(machine)) {
        return false;
    }

    if (!k10_video_begin() || !ensure_runtime_allocations()) {
        return false;
    }

    reset_audio_state();
    game_started = 0;
    g_cached_buttons = 0;
    g_present_task = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake(pdTRUE, 0);

    prepare_emulation();
#ifndef SINGLE_MACHINE
    ::machine = (signed char)machine;
#endif

    // Match the reference startup behavior: parse registers and pre-render one
    // audio chunk so playback begins immediately once DMA accepts writes.
    audio_namco_waveregs_parse_cpp();
    snd_render_buffer_cpp();

    const BaseType_t task_ok = xTaskCreatePinnedToCore(emulation_task, "emulation task", kEmulationTaskStackWords,
                                                       nullptr, kEmulationTaskPriority, &g_emulation_task,
                                                       kEmulationTaskCore);
    if (task_ok != pdPASS) {
        teardown_emulation_task();
        return false;
    }

    // Pre-seed one notification so the emulation task's first ulTaskNotifyTake
    // (in emulate_frame) returns immediately without deadlocking, even for games
    // like 1942 that set game_started=1 on the very first frame before the
    // present task has had a chance to send any notifications back.
    xTaskNotifyGive(g_emulation_task);

    g_runtime_running = true;
    return true;
}

void k10_emulator_stop() {
    teardown_emulation_task();
    reset_audio_state();
    k10_audio_clear();
}

bool k10_emulator_is_running() {
    return g_runtime_running;
}

bool k10_emulator_run_frame(uint8_t input_state) {
    if (!g_runtime_running) {
        return false;
    }

    g_cached_buttons = input_state;
    update_screen_cpp();
    return true;
}