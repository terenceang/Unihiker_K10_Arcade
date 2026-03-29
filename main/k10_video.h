#ifndef K10_VIDEO_H
#define K10_VIDEO_H

#include <stdint.h>

bool k10_video_begin();
void k10_video_begin_frame();
// Writes the provided buffer to the display and swaps to the other internal buffer.
void k10_video_write(const uint16_t* colors, uint32_t len);
void k10_video_end_frame();

// Returns the currently available buffer for drawing.
uint16_t* k10_video_get_draw_buffer();

// Menu and UI rendering.
void k10_video_draw_menu_frame(int selection);
void k10_video_draw_machine_frame(int machine);
int k10_video_wrap_menu_selection(int selection_index, int delta);
const char* k10_video_menu_name(int selection_index);
K10Machine k10_video_menu_machine(int selection_index);

#endif