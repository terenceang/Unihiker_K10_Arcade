#ifndef K10_VIDEO_H
#define K10_VIDEO_H

#include <stdint.h>
#include "k10_state.h"

bool k10_video_begin();
bool k10_video_set_machine_clock(K10Machine machine);
void k10_video_begin_frame();
// Writes the provided buffer to the display and swaps to the other internal buffer.
void k10_video_write(const uint16_t* colors, uint32_t len);
void k10_video_end_frame();

// Returns the currently available buffer for drawing.
uint16_t* k10_video_get_draw_buffer();

// Clear the border area (outside 224×288) to black.
void k10_video_clear_border();

// Menu and UI rendering.
void k10_video_draw_menu_frame(int selection);
void k10_video_draw_machine_frame(int machine);
int k10_video_wrap_menu_selection(int selection_index, int delta);
int k10_video_menu_count();
const char* k10_video_menu_name(int selection_index);
K10Machine k10_video_menu_machine(int selection_index);

#endif