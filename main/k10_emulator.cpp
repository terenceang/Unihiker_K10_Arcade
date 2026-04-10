#include "k10_emulator.h"

#include <esp_system.h>
#include <esp_heap_caps.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/idf_additions.h"

#include "k10_hardware.h"
#include "k10_idf.h"
#include "k10_video.h"

extern "C" {
#include "arcade_core/config.h"

struct Z80;

void prepare_emulation(void);
void emulate_frame(void);
extern unsigned char* memory;
extern char game_started;
extern unsigned char soundregs[32];
extern unsigned char starcontrol;
extern signed char machine;
}

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

#define IO_EMULATION
#include "arcade_core/tileaddr.h"
#include "arcade_core/galaga.h"
#ifdef ENABLE_PACMAN
#include "arcade_core/pacman.h"
#endif

namespace {

constexpr uint32_t kFramePixels = K10_TFT_ACTIVE_WIDTH * K10_TFT_STRIP_HEIGHT;
constexpr uint32_t kFrameBytes = kFramePixels * sizeof(uint16_t);
constexpr uint64_t kFramePeriodUs = 16667;
constexpr uint32_t kEmulationTaskStackWords = 4096;
constexpr UBaseType_t kEmulationTaskPriority = 2;
constexpr BaseType_t kEmulationTaskCore = 0;

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

int16_t clamp_pcm16(int sample) {
    if (sample > 32767) return 32767;
    if (sample < -32768) return -32768;
    return static_cast<int16_t>(sample);
}

void render_line(short strip_row) {
    memset(frame_buffer, 0, kFrameBytes);
    unsigned short* const original_buffer = frame_buffer;

    const short row0 = strip_row * 2;
    const short row1 = strip_row * 2 + 1;

#ifdef ENABLE_PACMAN
    if (machine == K10_MACHINE_PACMAN) {
        pacman_render_row(row0);
        frame_buffer += K10_TFT_ACTIVE_WIDTH * 8;
        pacman_render_row(row1);
    } else
#endif
    {
        galaga_render_row(row0);
        frame_buffer += K10_TFT_ACTIVE_WIDTH * 8;
        galaga_render_row(row1);
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
// Pac-Man Namco WSG register layout (each reg holds one 4-bit nibble):
//   Voice ch: regs [ch*5 .. ch*5+4] = 20-bit freq accumulator (low..high nibble)
//             reg  [ch*5+5]          = volume (bits 3:0)
//   Waveform: upper 3 bits of the high-freq nibble (soundregs[ch*5+4] >> 1)
void audio_pacman_waveregs_parse() {
    for (int ch = 0; ch < 3; ++ch) {
        const int base = ch * 5;
        snd_volume[ch] = soundregs[base + 5] & 0x0f;
        if (!snd_volume[ch]) continue;

        snd_freq[ch]  = soundregs[base + 0];
        snd_freq[ch] |= soundregs[base + 1] << 4;
        snd_freq[ch] |= soundregs[base + 2] << 8;
        snd_freq[ch] |= soundregs[base + 3] << 12;
        snd_freq[ch] |= (soundregs[base + 4] & 0x01) << 16;
        snd_freq[ch] <<= 4;  // scale to match synthesis step size

        const int wave_sel = (soundregs[base + 4] >> 1) & 0x07;
        snd_wave[ch] = pacman_wavetable[wave_sel];
    }
}
#endif

void audio_namco_waveregs_parse_cpp() {
#ifdef ENABLE_PACMAN
    if (machine == K10_MACHINE_PACMAN) { audio_pacman_waveregs_parse(); return; }
#endif
#ifdef ENABLE_GALAGA
    audio_galaga_waveregs_parse();
#endif
}

void snd_render_buffer_cpp() {
    const int32_t vol_scale = (64 * AUDIO_VOLUME) / 100;
    
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

    const uint32_t frame_start = static_cast<uint32_t>(k10_micros());
    frame_count++;

    if (g_emulation_task != nullptr) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }

    const uint32_t now = k10_millis();
    if (now - last_fps_time >= 5000) {
        const float fps = last_fps_time == 0 ? 0.0f : (frame_count * 1000.0f) / (now - last_fps_time);
        printf("FPS: %.2f, Free Heap: %u\n", fps, esp_get_free_heap_size());
        frame_count = 0;
        last_fps_time = now;
    }

#ifdef ENABLE_PACMAN
    if (machine == K10_MACHINE_PACMAN) {
        pacman_prepare_frame();
    } else
#endif
    {
        galaga_prepare_frame();
    }

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
    if (machine == K10_MACHINE_GALAGA) {
        stars_scroll_y += 2 * star_speeds[starcontrol & 7];
    }
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
        sprite = static_cast<sprite_S*>(malloc(128 * sizeof(sprite_S)));
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
}

void teardown_emulation_task() {
    if (g_emulation_task != nullptr) {
        vTaskDelete(g_emulation_task);
        g_emulation_task = nullptr;
    }

    g_present_task = nullptr;

    if (memory != nullptr) {
        free(memory);
        memory = nullptr;
    }
    
    if (snd_buffer != nullptr) {
        free(snd_buffer);
        snd_buffer = nullptr;
    }

    g_runtime_running = false;
}

}  // namespace

extern "C" void galaga_trigger_sound_explosion(void) {
    if (game_started) {
        snd_boom_cnt = 2 * sizeof(galaga_sample_boom);
        snd_boom_ptr = reinterpret_cast<const signed char*>(galaga_sample_boom);
    }
}

extern "C" unsigned char buttons_get(void) {
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
    switch (machine) {
#ifdef ENABLE_PACMAN
        case K10_MACHINE_PACMAN: return true;
#endif
#ifdef ENABLE_GALAGA
        case K10_MACHINE_GALAGA: return true;
#endif
#ifdef ENABLE_DKONG
        case K10_MACHINE_DKONG: return true;
#endif
#ifdef ENABLE_FROGGER
        case K10_MACHINE_FROGGER: return true;
#endif
#ifdef ENABLE_DIGDUG
        case K10_MACHINE_DIGDUG: return true;
#endif
#ifdef ENABLE_1942
        case K10_MACHINE_1942: return true;
#endif
        default: return false;
    }
}

bool k10_emulator_start(K10Machine machine) {
    if (!k10_emulator_supports_machine(machine)) {
        return false;
    }

    teardown_emulation_task();

    if (!k10_video_begin() || !ensure_runtime_allocations()) {
        return false;
    }

    reset_audio_state();
    game_started = 0;
    g_cached_buttons = 0;
    g_present_task = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake(pdTRUE, 0);

    prepare_emulation();
    ::machine = (signed char)machine;

    const BaseType_t task_ok = xTaskCreatePinnedToCore(emulation_task, "emulation task", kEmulationTaskStackWords,
                                                       nullptr, kEmulationTaskPriority, &g_emulation_task,
                                                       kEmulationTaskCore);
    if (task_ok != pdPASS) {
        teardown_emulation_task();
        return false;
    }

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