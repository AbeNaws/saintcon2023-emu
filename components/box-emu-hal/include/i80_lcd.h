#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define NUM_ROWS_IN_FRAME_BUFFER 40

uint16_t make_color(uint8_t r, uint8_t g, uint8_t b);
uint16_t* get_vram0();
uint16_t* get_vram1();
uint8_t* get_frame_buffer0();
uint8_t* get_frame_buffer1();
void lcd_write_frame(const uint16_t x, const uint16_t y, const uint16_t width, const uint16_t height, const uint8_t *data);
void lcd_init();
void display_clear();

#ifdef __cplusplus
}
#endif

