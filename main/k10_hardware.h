#ifndef K10_HARDWARE_H
#define K10_HARDWARE_H

#include <stddef.h>
#include <stdint.h>

bool k10_prepare_expander();
void k10_audio_init();
void k10_audio_set_dkong_rate(bool is_dkong);
size_t k10_audio_write(const int16_t* samples, size_t size);
void k10_audio_clear();
void k10_hw_init();
uint8_t k10_read_inputs();

#endif