#include "../drivers/video_driver.h"
#include "../utf-8.h"  // твой шрифт/функция get_char

VideoMode vmode;

/* ---------- низкоуровневые операции ---------- */

static inline void write_pixel_bytes(volatile uint8_t* dst, uint32_t color, uint32_t bpp_bytes) {
    uint8_t c0 = color & 0xFF;
    uint8_t c1 = (color >> 8) & 0xFF;
    uint8_t c2 = (color >> 16) & 0xFF;
    uint8_t c3 = (color >> 24) & 0xFF;
    if (bpp_bytes == 1) {
        dst[0] = c0;
    } else if (bpp_bytes == 2) {
        dst[0] = c0; dst[1] = c1;
    } else if (bpp_bytes == 3) {
        dst[0] = c0; dst[1] = c1; dst[2] = c2;
    } else {
        dst[0] = c0; dst[1] = c1; dst[2] = c2; dst[3] = c3;
    }
}

void put_pixel(int x, int y, uint32_t color) {
    if ((unsigned)x >= vmode.width || (unsigned)y >= vmode.height) return;
    uint8_t* p = vmode.fb + y * vmode.pitch + x * (vmode.bpp / 8);
    *(uint32_t*)p = color;
}

/* ---------- примитивы ---------- */

static inline int my_abs(int x) { return x < 0 ? -x : x; }

void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = my_abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -my_abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = err << 1;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void clear_line(uint32_t y, uint32_t color) {
    uint32_t bpp_bytes = vmode.bpp / 8;
    volatile uint8_t* line = vmode.fb + (uint64_t)y * vmode.pitch;
    for (uint32_t x = 0; x < vmode.width; x++) {
        write_pixel_bytes(line + x * bpp_bytes, color, bpp_bytes);
    }
}

void clear_screen(uint32_t color) {
    for (uint32_t y = 0; y < vmode.height; ++y) {
        clear_line(y, color);
    }
}

/* ---------- текст ---------- */

void draw_char(int x, int y, const uint8_t *bitmap, uint32_t fg) {
    for (int row = 0; row < CHAR_H; row++) {
        uint8_t bits = bitmap[row];
        for (int col = 0; col < CHAR_W; col++) {
            if (bits & (1u << (CHAR_W - 1 - col))) {
                put_pixel(x + col, y + row, fg);
            }
        }
    }
}

void print_char_xy(int x, int y, char c, uint32_t color) {
    const uint8_t* bm = get_char((char16_t)c);
    draw_char(x, y, bm, color);
}

void print_xy(int x, int y, const char* s, uint32_t color) {
    int cx = x;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '\n') { y += CHAR_H + 1; cx = x; continue; }
        print_char_xy(cx, y, s[i], color);
        cx += CHAR_W + CHAR_SPACING;
    }
}

/* ---------- init ---------- */

void init_vbe_driver(uint32_t phys_base_ptr, uint32_t width, uint32_t height, uint32_t bpp, uint32_t pitch) {
    vmode.fb = (volatile uint8_t*)(uintptr_t)phys_base_ptr;
    vmode.width = width;
    vmode.height = height;
    vmode.bpp = bpp;
    vmode.pitch = pitch;
}

void draw_bitmap32(const uint32_t* bmp, int bmp_w, int bmp_h, int x0, int y0) {
    for (int y = 0; y < bmp_h; y++) {
        for (int x = 0; x < bmp_w; x++) {
            uint32_t color = bmp[y * bmp_w + x];
            put_pixel(x0 + x, y0 + y, color);
        }
    }
}

void draw_cube(int bmp_w, int bmp_h, int x0, int y0, uint32_t color) {
    for (int y = 0; y < bmp_h; y++) {
        for (int x = 0; x < bmp_w; x++) {
            put_pixel(x0 + x, y0 + y, color);
        }
    }
}
