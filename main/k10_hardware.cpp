#include "k10_hardware.h"

#include "driver/i2c.h"
#include "driver/i2s_std.h"

#include "k10_idf.h"
#include "k10_config.h"
#include "k10_input.h"

#ifndef I2S_MCLK_MULTIPLE_DEFAULT
#define I2S_MCLK_MULTIPLE_DEFAULT I2S_MCLK_MULTIPLE_256
#endif

namespace {

constexpr i2c_port_t kI2CPort = I2C_NUM_0;
constexpr uint32_t kI2CFrequencyHz = 400000;
constexpr TickType_t kI2CTimeoutTicks = pdMS_TO_TICKS(50);
constexpr size_t kAudioSilenceFrames = 64;

bool g_i2c_ready = false;
bool g_i2s_ready = false;
i2s_chan_handle_t g_i2s_tx = nullptr;
uint32_t g_i2s_sample_rate = K10_GALAGA_AUDIO_SAMPLE_RATE;
uint8_t g_expander_output_port1 = K10_EXPANDER_OUTPUT_PORT1;

i2s_std_clk_config_t make_i2s_clock_config(uint32_t sample_rate_hz) {
    i2s_std_clk_config_t clk_config = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate_hz);
    clk_config.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    return clk_config;
}

void reset_i2s_state() {
    if (g_i2s_tx != nullptr) {
        i2s_del_channel(g_i2s_tx);
        g_i2s_tx = nullptr;
    }
    g_i2s_ready = false;
}

bool ensure_i2c_ready() {
    if (g_i2c_ready) {
        return true;
    }

    i2c_config_t config = {};
    config.mode = I2C_MODE_MASTER;
    config.sda_io_num = static_cast<gpio_num_t>(K10_I2C_SDA);
    config.scl_io_num = static_cast<gpio_num_t>(K10_I2C_SCL);
    config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    config.scl_pullup_en = GPIO_PULLUP_ENABLE;
    config.master.clk_speed = kI2CFrequencyHz;

    if (i2c_param_config(kI2CPort, &config) != ESP_OK) {
        return false;
    }

    const esp_err_t result = i2c_driver_install(kI2CPort, config.mode, 0, 0, 0);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return false;
    }

    g_i2c_ready = true;
    return true;
}

bool write_register(uint8_t address, uint8_t reg, uint8_t value) {
    if (!ensure_i2c_ready()) {
        return false;
    }

    const uint8_t data[] = {reg, value};
    return i2c_master_write_to_device(kI2CPort, address, data, sizeof(data), kI2CTimeoutTicks) == ESP_OK;
}

bool read_registers(uint8_t address, uint8_t reg, uint8_t* buffer, size_t length) {
    if (buffer == nullptr || length == 0) {
        return false;
    }

    if (!ensure_i2c_ready()) {
        return false;
    }

    return i2c_master_write_read_device(kI2CPort, address, &reg, sizeof(reg), buffer, length, kI2CTimeoutTicks) ==
           ESP_OK;
}

bool read_expander_inputs(uint8_t* port0, uint8_t* port1) {
    uint8_t ports[2] = {0, 0};
    if (!read_registers(K10_EXPANDER_ADDR, XL9535_INPUT_REG_0, ports, sizeof(ports))) {
        return false;
    }

    *port0 = ports[0];
    *port1 = ports[1];
    return true;
}

bool write_expander_port1(uint8_t value) {
    if (!write_register(K10_EXPANDER_ADDR, XL9535_OUTPUT_REG_1, value)) {
        return false;
    }

    g_expander_output_port1 = value;
    return true;
}

bool set_user_led(bool enabled) {
    if (!k10_prepare_expander()) {
        return false;
    }

    uint8_t next_value = g_expander_output_port1;
    if (enabled) {
        next_value |= K10_MASK_USER_LED;
    } else {
        next_value &= static_cast<uint8_t>(~K10_MASK_USER_LED);
    }

    return write_expander_port1(next_value);
}

}  // namespace

bool k10_prepare_expander() {
    static bool expander_prepared = false;

    if (expander_prepared) {
        return true;
    }

    const bool port0_configured = write_register(K10_EXPANDER_ADDR, XL9535_CONFIG_REG_0, K10_EXPANDER_CONFIG_PORT0);
    const bool port1_configured = write_register(K10_EXPANDER_ADDR, XL9535_CONFIG_REG_1, K10_EXPANDER_CONFIG_PORT1);
    const bool port0_output_set = write_register(K10_EXPANDER_ADDR, XL9535_OUTPUT_REG_0, K10_EXPANDER_OUTPUT_PORT0);
    const bool port1_output_set = write_expander_port1(K10_EXPANDER_OUTPUT_PORT1);

    expander_prepared = port0_configured && port1_configured && port0_output_set && port1_output_set;
    return expander_prepared;
}

void k10_audio_init() {
    if (g_i2s_ready) {
        return;
    }

    i2s_chan_config_t chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_config.dma_desc_num = 4;
    chan_config.dma_frame_num = kAudioSilenceFrames;
    chan_config.auto_clear = true;
    chan_config.intr_priority = 2;

    if (i2s_new_channel(&chan_config, &g_i2s_tx, nullptr) != ESP_OK) {
        printf("Audio: I2S channel allocation failed\n");
        return;
    }

    i2s_std_config_t std_config = {
        .clk_cfg = make_i2s_clock_config(K10_GALAGA_AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = static_cast<gpio_num_t>(K10_I2S_MCLK),
            .bclk = static_cast<gpio_num_t>(K10_I2S_BCK),
            .ws = static_cast<gpio_num_t>(K10_I2S_WS),
            .dout = static_cast<gpio_num_t>(K10_I2S_DOUT),
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {},
        },
    };

    if (i2s_channel_init_std_mode(g_i2s_tx, &std_config) != ESP_OK) {
        printf("Audio: I2S standard mode init failed\n");
        reset_i2s_state();
        return;
    }

    if (i2s_channel_enable(g_i2s_tx) != ESP_OK) {
        printf("Audio: I2S channel enable failed\n");
        reset_i2s_state();
        return;
    }

    printf("Audio: I2S pins configured (BCK:%d, WS:%d, DO:%d, MCLK:%d)\n", K10_I2S_BCK, K10_I2S_WS,
           K10_I2S_DOUT, K10_I2S_MCLK);
    g_i2s_sample_rate = K10_GALAGA_AUDIO_SAMPLE_RATE;
    g_i2s_ready = true;
    k10_audio_clear();
}

void k10_audio_set_dkong_rate(bool is_dkong) {
    if (!g_i2s_ready || g_i2s_tx == nullptr) {
        return;
    }

    const uint32_t sample_rate = is_dkong ? K10_DKONG_AUDIO_SAMPLE_RATE : K10_GALAGA_AUDIO_SAMPLE_RATE;
    if (sample_rate == g_i2s_sample_rate) {
        return;
    }

    if (i2s_channel_disable(g_i2s_tx) != ESP_OK) {
        printf("Audio: I2S channel disable failed\n");
        return;
    }

    const i2s_std_clk_config_t clk_config = make_i2s_clock_config(sample_rate);
    if (i2s_channel_reconfig_std_clock(g_i2s_tx, &clk_config) != ESP_OK) {
        printf("Audio: I2S sample rate reconfig failed\n");
        i2s_channel_enable(g_i2s_tx);
        return;
    }

    if (i2s_channel_enable(g_i2s_tx) != ESP_OK) {
        printf("Audio: I2S channel re-enable failed\n");
        return;
    }

    g_i2s_sample_rate = sample_rate;
    k10_audio_clear();
}

size_t k10_audio_write(const int16_t* samples, size_t size) {
    if (!g_i2s_ready || g_i2s_tx == nullptr || samples == nullptr || size == 0) {
        return 0;
    }

    size_t bytes_written = 0;
    if (i2s_channel_write(g_i2s_tx, samples, size, &bytes_written, 0) != ESP_OK) {
        return 0;
    }

    return bytes_written;
}

void k10_audio_clear() {
    if (!g_i2s_ready || g_i2s_tx == nullptr) {
        return;
    }

    static const int16_t silence_buffer[kAudioSilenceFrames * 2] = {0};
    size_t bytes_written = 0;
    i2s_channel_write(g_i2s_tx, silence_buffer, sizeof(silence_buffer), &bytes_written, 0);
}

void k10_hw_init() {
    k10_prepare_expander();

    k10_audio_init();
}

uint8_t k10_read_inputs() {
    uint8_t input_states = 0;

#if !K10_DISABLE_ONBOARD_BUTTONS
    // The K10 only has 2 primary user buttons (A/B) on the expander.
    // We map these to FIRE and COIN, and their combo to START.
    uint8_t port0 = 0;
    uint8_t port1 = 0;

    if (read_expander_inputs(&port0, &port1)) {
        const bool key_a_pressed = (port1 & K10_MASK_KEY_A) == 0;
        const bool key_b_pressed = (port0 & K10_MASK_KEY_B) == 0;

        if (key_a_pressed && key_b_pressed) {
            input_states |= K10_BUTTON_START;
        } else {
            if (key_a_pressed) {
                input_states |= K10_BUTTON_FIRE;
            }
            if (key_b_pressed) {
                input_states |= K10_BUTTON_COIN;
            }
        }
    }
#endif

    return input_states;
}