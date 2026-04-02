#ifndef CONSOLE_APP_H
#define CONSOLE_APP_H

#include <efi.h>
#include <efilib.h>
#include <stdbool.h>
#include "drivers/keyboard_driver.h"
#include "drivers/video_driver_uefi.h"

UINT64 MAX_X = 80;
UINT64 MAX_Y = 25;

EFI_SYSTEM_TABLE *SystemTable;
bool mode_gnu = false;
int8_t pos_x = 0, pos_y = 0;
// -------------------------
static void scroll_if_needed();
void console_init(EFI_SYSTEM_TABLE *SystemTable_ptr, bool mode_gnu_) {
    SystemTable = SystemTable_ptr;
    mode_gnu = mode_gnu_;

    if (mode_gnu) {
        if (vmode.width > 0 && vmode.height > 0) {
            MAX_X = (vmode.width / (CHAR_W + CHAR_SPACING))*2;
            MAX_Y = vmode.height / (CHAR_H + CHAR_SPACING);
        } else {
            // fallback — если vmode не инициализирован, оставляем текстовый режим UEFI
            Print(L"Warning: GOP mode not detected, switching to text mode.\n");
            MAX_X = 80;
            MAX_Y = 25;
        }
    } else {
        Print(L"Starting to text mode.\n");
        // В стандартном текстовом режиме UEFI
        MAX_X = 80;
        MAX_Y = 25;
    }
}

// -------------------------
// Очистка экрана
// -------------------------
void console_clear_screen(UINT32 color) {
    if (mode_gnu) {
        clear_screen(color);
    } else {
        SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    }
    set_cursor_pos(0,0);
}

// -------------------------
// Установка позиции курсора
// -------------------------
void set_cursor_pos(int8_t x, int8_t y) {
    pos_x = x;
    pos_y = y;
    if (!mode_gnu) {
        SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, pos_x, pos_y);
    }
}

// -------------------------
// Скроллинг текста (только для текстовой консоли)

// -------------------------
// Вывод одиночного символа
// -------------------------
void console_print_char(CHAR16 ch, UINT32 color) {
    scroll_if_needed();
    if (mode_gnu) {
        print_char_xy(
            ((CHAR_W+CHAR_SPACING) * pos_x) + CHAR_SPACING,
            ((CHAR_H+CHAR_SPACING) * pos_y) + CHAR_SPACING,
            ch,
            color
        );
    } else {
        SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, pos_x, pos_y);
        SystemTable->ConOut->SetAttribute(SystemTable->ConOut, color & 0x0F);
        Print(L"%c", ch);
    }

    pos_x++;
    if (pos_x >= MAX_X) {
        pos_x = 0;
        pos_y++;
        scroll_if_needed();
    }
    set_cursor_pos(pos_x, pos_y);
}

// -------------------------
// Вывод строки
// -------------------------
void console_print(CHAR16 *str, UINT32 color) {
    scroll_if_needed();
    if (!str) return;
    
    while (*str) {
        if (*str == L'\n') {
            pos_x = 0;
            pos_y++;
        } else {
            console_print_char(*str, color);
        }
        str++;
    }
}

// -------------------------
// Чтение символа с клавиатуры
// -------------------------
CHAR16 console_ReadChar() {
    return ReadChar(SystemTable);
}

// -------------------------
// Чтение строки с клавиатуры (Backspace, Enter)
// -------------------------
CHAR16* console_ReadLine() {
    scroll_if_needed();
    static CHAR16 buffer[256];
    int idx = 0;
    CHAR16 ch;

    // Устанавливаем курсор для начала ввода
    set_cursor_pos(pos_x, pos_y);

    while (1) {
        ch = console_ReadChar(); // ждем символ

        // Enter (новая строка)
        if (ch == CHAR_CARRIAGE_RETURN || ch == L'\n') {
            buffer[idx] = L'\0';
            console_print(L"\n", White); // новая строка
            break;
        }

        // Backspace
        else if (ch == CHAR_BACKSPACE) {
            if (idx > 0) {
                idx--;
                // двигаем курсор назад
                pos_x--;
                if (pos_x < 0) {
                    pos_x = MAX_X - 1;
                    pos_y--;
                    if (pos_y < 0) pos_y = 0;
                }
                set_cursor_pos(pos_x, pos_y);
                console_print_char(buffer[idx],Black); // затираем символ
                pos_x--;
                set_cursor_pos(pos_x, pos_y);
            }
        }
        // Обычный символ
        else {
            if (idx < 255) {
                buffer[idx++] = ch;
                console_print_char(ch, White); // печатаем сразу в реальном времени
            }
        }
    }

    return buffer;
}
// -------------------------
// Реальный скроллинг вверх
// -------------------------
// color_bg пока не используется для text-mode, но оставим параметр для совместимости
void console_scroll_up(UINT32 color_bg) {
    if (mode_gnu) {
        // В графическом режиме – используем драйвер (реализация scroll_screen_up должна быть в video_driver_uefi.c)
        scroll_screen_up(10);
    } else {
        // В текстовом режиме UEFI — делаем "переход" на следующую строку, чтобы драйвер сам прокрутил.
        // UEFI не предоставляет API для чтения/сдвига буфера, поэтому самый простой способ —
        // вывести пустую строку (OutputString с \n) или использовать ClearScreen+перерисовку своего буфера.
        // Здесь делаем простую безопасную реализацию: печатаем '\r\n' MAX_Y разолепительно, 
        // и затем ставим курсор в последнюю строку.
        // (Если нужен настоящий скролл в text-mode — придется держать свой screen_buf и перерисовывать.)
        SystemTable->ConOut->SetCursorPosition(SystemTable->ConOut, 0, MAX_Y - 1);
        // заполнить последнюю строку пробелами
        for (UINTN i = 0; i < MAX_X; i++) {
            Print(L" ");
        }
    }

    // После скролла — ставим курсор в начало последней видимой строки
    pos_y = (int8_t)((MAX_Y > 0) ? (MAX_Y - 1) : 0);
    pos_x = 0;
    set_cursor_pos(pos_x, pos_y);
}

// -------------------------
// Проверка и автоскролл
// -------------------------
// Вызывать ТОЛЬКО когда позиция курсора изменилась.
// Скроллим только если pos_y вышло за пределы MAX_Y-1.
static void scroll_if_needed() {
    // Приведение типов — чтобы избежать варнингов при сравнении signed/unsigned
    if ((UINT64)pos_y >= MAX_Y) {
        console_scroll_up(Black);  // цвет фона (например, чёрный)
    }
}

#endif // CONSOLE_APP_H
