#ifndef VIDEO_DRIVER_UEFI_H
#define VIDEO_DRIVER_UEFI_H

#include <efi.h>
#include <stdint.h>



/* сохраняю твою метрику шрифта */
#define CHAR_W 5
#define CHAR_H 7
#define CHAR_SPACING 1

/* палитра (24-bit RGB) */
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



typedef struct {
    UINT32 width;
    UINT32 height;
    UINT32 bpp;           // bits per pixel (usually 32)
    UINT32 pitch;         // bytes per scanline
    volatile UINT8* fb;   // framebuffer base
    EFI_GRAPHICS_PIXEL_FORMAT pixel_format;
} VideoMode;

/* глобальная структура режима */
extern VideoMode vmode;
/* API */
EFI_STATUS init_gop_driver(EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS init_gop_from_protocol(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop);
void scroll_screen_up(int speed_scroll);
void put_pixel(INT32 x, INT32 y, UINT32 rgb24); // rgb24: 0xRRGGBB
void draw_line(INT32 x0, INT32 y0, INT32 x1, INT32 y1, UINT32 rgb24);
void clear_screen(UINT32 rgb24);

void draw_crosshair(void); // для теста

void draw_char(INT32 x, INT32 y, const uint8_t *bitmap, UINT32 fg);
void print_char_xy(INT32 x, INT32 y, CHAR16 c, UINT32 color);
void print_xy(INT32 x, INT32 y, const CHAR16* s, UINT32 color);

void draw_bitmap32(const UINT32* bmp, INT32 bmp_w, INT32 bmp_h, INT32 x0, INT32 y0);
void draw_block(INT32 bmp_w, INT32 bmp_h, INT32 x0, INT32 y0, UINT32 rgb24);


#endif // VIDEO_DRIVER_UEFI_H
