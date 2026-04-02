#ifndef VIDEO_DRIVER_H
#define VIDEO_DRIVER_H
#include <stdint.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;           // бит на пиксель: 8/15/16/24/32
    uint32_t pitch;         // байт на строку (stride)
    volatile uint8_t* fb;   // линейный адрес framebuffer
} VideoMode;

#define CHAR_W 5
#define CHAR_H 7
#define CHAR_SPACING 1

#define Black      0x000000
#define White      0xFFFFFF
#define Grey       0xAAAAAA
#define DarkGrey   0x555555
#define Red        0xFF0000
#define DarkRed    0x800000
#define Green      0x00FF00
#define DarkGreen  0x008000
#define Blue       0x0000FF
#define DarkBlue   0x000080
#define Yellow     0xFFFF00
#define DarkYellow 0x808000
#define Cyan       0x00FFFF
#define DarkCyan   0x008080
#define Magenta    0xFF00FF
#define DarkMagenta 0x800080

// Дополнительные 16 цветов (для расширенной палитры)
#define Orange     0xFFA500
#define DarkOrange 0xFF8C00
#define Pink       0xFFC0CB
#define Purple     0x800080
#define Brown      0x8B4513
#define LightBrown 0xA0522D
#define LightBlue  0xADD8E6
#define LightGreen 0x90EE90
#define Lime       0x00FF00
#define Indigo     0x4B0082
#define Violet     0xEE82EE
#define Olive      0x808000
#define Maroon     0x800000
#define Teal       0x008080
#define Navy       0x000080
#define Coral      0xFF7F50
/* глобальная структура режима */
extern VideoMode vmode;

/* ----- API ----- */
void init_vbe_driver(uint32_t phys_base_ptr, uint32_t width, uint32_t height, uint32_t bpp, uint32_t pitch);

void put_pixel(int x, int y, uint32_t color);
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void clear_screen(uint32_t color);

void print_char_xy(int x, int y, char c, uint32_t color);
void print_xy(int x, int y, const char* s, uint32_t color);

void draw_char(int x, int y, const uint8_t *bitmap, uint32_t fg);
void draw_bitmap32(const uint32_t* bmp, int bmp_w, int bmp_h, int x0, int y0);;
void draw_cube(int bmp_w, int bmp_h, int x0, int y0, uint32_t color);

#endif
