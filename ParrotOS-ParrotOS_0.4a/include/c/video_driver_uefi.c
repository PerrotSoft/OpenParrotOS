#include "../drivers/video_driver_uefi.h"
#include "../utf-8.h"
#include <string.h>
#include <efilib.h>
/* глобальная */
VideoMode vmode = {0};

/* Вспом. упаковать 0xRRGGBB -> порядок байт для фреймбуфера (в зависимости от PixelFormat) */
static inline void pack_rgb24_to_fb(UINT32 rgb24, volatile UINT8 *dst, UINT32 bpp_bytes, EFI_GRAPHICS_PIXEL_FORMAT fmt)
{
    UINT8 r = (rgb24 >> 16) & 0xFF;
    UINT8 g = (rgb24 >> 8) & 0xFF;
    UINT8 b = (rgb24) & 0xFF;

    if (bpp_bytes == 1) {
        dst[0] = b; // палитровый fallback
    } else if (bpp_bytes == 2) {
        // 16-bit 5:6:5
        UINT16 p = (UINT16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        dst[0] = (UINT8)(p & 0xFF);
        dst[1] = (UINT8)(p >> 8);
    } else if (bpp_bytes == 3) {
        if (fmt == PixelRedGreenBlueReserved8BitPerColor) {
            dst[0] = r; dst[1] = g; dst[2] = b;
        } else {
            dst[0] = b; dst[1] = g; dst[2] = r;
        }
    } else { // 4 bytes
        if (fmt == PixelRedGreenBlueReserved8BitPerColor) {
            dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = 0;
        } else {
            dst[0] = b; dst[1] = g; dst[2] = r; dst[3] = 0;
        }
    }
}

/* безопасная запись пикселя через vmode */
void put_pixel(INT32 x, INT32 y, UINT32 rgb24)
{
    if (!vmode.fb) return;
    if (x < 0 || y < 0) return;
    if ((UINT32)x >= vmode.width || (UINT32)y >= vmode.height) return;

    UINT32 bpp_bytes = vmode.bpp / 8;
    volatile UINT8 *dst = vmode.fb + (UINT64)y * vmode.pitch + (UINT64)x * bpp_bytes;
    pack_rgb24_to_fb(rgb24, (UINT8*)dst, bpp_bytes, vmode.pixel_format);
}

void scroll_screen_up(int speed_scroll) {
    if (!vmode.fb) return;

    const UINT32 shift_px = CHAR_W+CHAR_SPACING;                 // фиксированный сдвиг в пикселях
    UINT32 height = vmode.height;
    UINT32 pitch  = vmode.pitch;                // байт на строку (строчный pitch!)
    UINT32 width  = vmode.width;
    UINT32 bpp_bytes = vmode.bpp / 8;

    if (pitch == 0 || height == 0 || width == 0 || bpp_bytes == 0) return;
    if (shift_px >= height) {
        // если сдвиг >= высоты — просто очистим экран
        for (UINT32 y = 0; y < height; y+=speed_scroll) {
            volatile UINT8 *line = vmode.fb + (UINT64)y * pitch;
            for (UINT32 x = 0; x < width; x++) {
                pack_rgb24_to_fb(0x000000, (UINT8*)(line + (UINT64)x * bpp_bytes), bpp_bytes, vmode.pixel_format);
            }
        }
        return;
    }

    volatile UINT8 *fb = vmode.fb;
    UINT32 rows_to_move = height - shift_px;

    // Копируем построчно: для dest_row = 0..rows_to_move-1 берем src_row = dest_row + shift_px
    // Используем memcpy на каждую строку (безопасно и понятно)
    for (UINT32 row = 0; row < rows_to_move; row++) {
        void *dst = (void*)(fb + (UINT64)row * pitch);
        const void *src = (const void*)(fb + (UINT64)(row + shift_px) * pitch);
        memcpy(dst, src, (size_t)pitch);
    }

    // Очистить нижние shift_px строк (залить чёрным; при желании заменить цвет)
    const UINT32 clear_color = 0x000000;
    for (UINT32 row = rows_to_move; row < height; row++) {
        volatile UINT8 *line = fb + (UINT64)row * pitch;
        for (UINT32 x = 0; x < width; x++) {
            pack_rgb24_to_fb(clear_color, (UINT8*)(line + (UINT64)x * bpp_bytes), bpp_bytes, vmode.pixel_format);
        }
    }
}
/* Bresenham */
static inline INT32 abs_i(INT32 v) { return v < 0 ? -v : v; }
void draw_line(INT32 x0, INT32 y0, INT32 x1, INT32 y1, UINT32 rgb24)
{
    INT32 dx = abs_i(x1 - x0), sx = x0 < x1 ? 1 : -1;
    INT32 dy = -abs_i(y1 - y0), sy = y0 < y1 ? 1 : -1;
    INT32 err = dx + dy, e2;
    for (;;) {
        put_pixel(x0, y0, rgb24);
        if (x0 == x1 && y0 == y1) break;
        e2 = err << 1;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void clear_screen(UINT32 rgb24)
{
    if (!vmode.fb) return;
    UINT32 bpp_bytes = vmode.bpp / 8;
    for (UINT32 y = 0; y < vmode.height; ++y) {
        volatile UINT8 *line = vmode.fb + (UINT64)y * vmode.pitch;
        for (UINT32 x = 0; x < vmode.width; ++x) {
            pack_rgb24_to_fb(rgb24, (UINT8*)(line + x * bpp_bytes), bpp_bytes, vmode.pixel_format);
        }
    }
}

void draw_crosshair(void)
{
    if (!vmode.fb) return;
    UINT32 cx = vmode.width / 2;
    UINT32 cy = vmode.height / 2;
    draw_line((INT32)cx - 50, (INT32)cy, (INT32)cx + 50, (INT32)cy, 0xFF0000); // красная
    draw_line((INT32)cx, (INT32)cy - 50, (INT32)cx, (INT32)cy + 50, 0x00FF00); // зелёная
}

/* Инициализация из уже найденного протокола GOP:
   - перебираем available modes,
   - выбираем первый валидный (ненулевое разрешение),
   - устанавливаем mode, читаем FrameBufferBase и заполняем vmode.
*/
EFI_STATUS init_gop_from_protocol(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop)
{
    if (!Gop) return EFI_INVALID_PARAMETER;

    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN InfoSize;

    for (UINT32 i = 0; i < Gop->Mode->MaxMode; ++i) {
        Status = uefi_call_wrapper(Gop->QueryMode, 4, Gop, i, &InfoSize, &Info);
        if (EFI_ERROR(Status)) continue;
        if (Info->HorizontalResolution == 0 || Info->VerticalResolution == 0) continue;

        Status = uefi_call_wrapper(Gop->SetMode, 2, Gop, i);
        if (EFI_ERROR(Status)) continue;

        // теперь гарантия, что Mode и Info валидны
        Status = uefi_call_wrapper(Gop->QueryMode, 4, Gop, Gop->Mode->Mode, &InfoSize, &Info);
        if (EFI_ERROR(Status)) return Status;

        vmode.width = Info->HorizontalResolution;
        vmode.height = Info->VerticalResolution;
        vmode.bpp = 32; // обычно 32
        vmode.pitch = Info->PixelsPerScanLine * (vmode.bpp / 8);
        vmode.fb = (volatile UINT8*)(UINTN)Gop->Mode->FrameBufferBase;
        vmode.pixel_format = Info->PixelFormat;
        return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED;
}

/* Инициализация по SystemTable: найди GOP через LocateProtocol и вызови init_gop_from_protocol */
EFI_STATUS init_gop_driver(EFI_SYSTEM_TABLE *SystemTable)
{
    if (!SystemTable) return EFI_INVALID_PARAMETER;

    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    Status = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3, &gopGuid, NULL, (void**)&Gop);
    if (EFI_ERROR(Status)) return Status;

    return init_gop_from_protocol(Gop);
}
void draw_char(INT32 x, INT32 y, const uint8_t *bitmap, UINT32 fg) {
    for (INT32 row = 0; row < CHAR_H; row++) {
        uint8_t bits = bitmap[row];
        for (INT32 col = 0; col < CHAR_W; col++) {
            if (bits & (1u << (CHAR_W - 1 - col))) {
                put_pixel(x + col, y + row, fg);
            }
        }
    }
}

void print_char_xy(INT32 x, INT32 y, CHAR16 c, UINT32 color) {
    const uint8_t* bm = get_char(c); // твоя функция должна вернуть uint8_t[CHAR_H]
    if (!bm) return;
    draw_char(x, y, bm, color);
}

void print_xy(INT32 x, INT32 y, const CHAR16* s, UINT32 color) {
    INT32 cx = x;
    for (UINTN i = 0; s[i]; i++) {
        if (s[i] == L'\n') { y += CHAR_H + 1; cx = x; continue; }
        print_char_xy(cx, y, s[i], color);
        cx += CHAR_W + CHAR_SPACING;
    }
}

/* Битмап и блоки */
void draw_bitmap32(const UINT32* bmp, INT32 bmp_w, INT32 bmp_h, INT32 x0, INT32 y0) {
    for (INT32 y = 0; y < bmp_h; y++) {
        for (INT32 x = 0; x < bmp_w; x++) {
            UINT32 rgb = bmp[y * bmp_w + x]; // ожидаем 0xRRGGBB
            put_pixel(x0 + x, y0 + y, rgb);
        }
    }
}

void draw_block(INT32 bmp_w, INT32 bmp_h, INT32 x0, INT32 y0, UINT32 rgb24) {
    for (INT32 y = 0; y < bmp_h; y++) {
        for (INT32 x = 0; x < bmp_w; x++) {
            put_pixel(x0 + x, y0 + y, rgb24);
        }
    }
}