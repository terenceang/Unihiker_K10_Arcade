#ifndef K10_GAME_REGISTRY_H
#define K10_GAME_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

#include "k10_config.h"
#include "state/k10_state.h"

struct K10GameRuntimeConfig {
    K10Machine machine;
    uint32_t spi_clock_hz;
    bool use_dkong_audio_rate;
};

static constexpr K10GameRuntimeConfig k10_game_runtime_configs[] = {
#ifdef ENABLE_PACMAN
    {K10_MACHINE_PACMAN, K10_TFT_SPICLK, false},
#endif
#ifdef ENABLE_GALAGA
    {K10_MACHINE_GALAGA, K10_TFT_SPICLK, false},
#endif
#ifdef ENABLE_DKONG
    {K10_MACHINE_DKONG, K10_TFT_SPICLK, true},
#endif
#ifdef ENABLE_FROGGER
    {K10_MACHINE_FROGGER, K10_TFT_SPICLK, false},
#endif
#ifdef ENABLE_DIGDUG
    {K10_MACHINE_DIGDUG, K10_TFT_SPICLK, false},
#endif
#ifdef ENABLE_1942
    {K10_MACHINE_1942, K10_TFT_SPICLK, false},
#endif
};

static constexpr size_t k10_game_runtime_config_count =
    sizeof(k10_game_runtime_configs) / sizeof(k10_game_runtime_configs[0]);

static constexpr const K10GameRuntimeConfig* k10_find_game_runtime_config(K10Machine machine) {
    for (size_t index = 0; index < k10_game_runtime_config_count; ++index) {
        if (k10_game_runtime_configs[index].machine == machine) {
            return &k10_game_runtime_configs[index];
        }
    }
    return nullptr;
}

static constexpr K10Machine k10_default_enabled_machine() {
    return k10_game_runtime_config_count > 0 ? k10_game_runtime_configs[0].machine : K10_MACHINE_MENU;
}

static constexpr bool k10_machine_is_enabled(K10Machine machine) {
    return k10_find_game_runtime_config(machine) != nullptr;
}

static constexpr uint32_t k10_machine_spi_clock_hz(K10Machine machine) {
    const K10GameRuntimeConfig* config = k10_find_game_runtime_config(machine);
    return config != nullptr ? config->spi_clock_hz : K10_TFT_SPICLK;
}

static constexpr bool k10_machine_uses_dkong_audio_rate(K10Machine machine) {
    const K10GameRuntimeConfig* config = k10_find_game_runtime_config(machine);
    return config != nullptr && config->use_dkong_audio_rate;
}

#endif