#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdint.h>
#include "../io.h" // чтобы inb/outb были известны
#include "../pit.h"
#include <stdbool.h>
#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CAPSLOCK 0x3A
#define SC_ESC      0x01

// F-клавиши
#define SC_F1   0x3B
#define SC_F2   0x3C
#define SC_F3   0x3D
#define SC_F4   0x3E
#define SC_F5   0x3F
#define SC_F6   0x40
#define SC_F7   0x41
#define SC_F8   0x42
#define SC_F9   0x43
#define SC_F10  0x44

// Стрелки и спец. клавиши (идут после 0xE0 префикса)
#define SC_UP       0x48
#define SC_DOWN     0x50
#define SC_LEFT     0x4B
#define SC_RIGHT    0x4D
#define SC_PGUP     0x49
#define SC_PGDOWN   0x51
#define SC_HOME     0x47
#define SC_END      0x4F
#define SC_INSERT   0x52
#define SC_DELETE   0x53

bool shift_pressed = false;
bool caps_lock = false;
static const char scancode_table[] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',18,' ',0,SC_F1,SC_F2,SC_F3,SC_F4,SC_F5,SC_F6,SC_F7,SC_F8,SC_F9,SC_F10
    ,0,0,0,SC_UP,SC_PGUP,0,SC_LEFT,0,SC_RIGHT,0,0,SC_DOWN
};

static const char scancode_shift_table[] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,
    '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',18,' ','0',SC_F1,SC_F2,SC_F3,SC_F4,SC_F5,SC_F6,SC_F7,SC_F8,SC_F9,SC_F10
    ,0,0,0,SC_UP,SC_PGUP,0,SC_LEFT,0,SC_RIGHT,0,0,SC_DOWN
};

char process_scancode(uint8_t scancode) {
    uint8_t code = scancode & 0x7F;
    if (scancode==SC_CAPSLOCK)
        shift_pressed=!shift_pressed;
        if (scancode==SC_ESC)
        return SC_ESC;
    if (code >= sizeof(scancode_table))
        return 0;

    char c;

    // Логика выбора таблицы для цифр и символов с Shift
    bool use_shift_table = shift_pressed;

    // Если буква — учитываем caps_lock и shift
    if (code >= 0x10 && code <= 0x39) { // диапазон букв примерно
        if (caps_lock) {
            // Если CapsLock включен, инвертируем эффект Shift для букв
            use_shift_table = !use_shift_table;
        }
    }
    use_shift_table = false;
    if (shift_pressed)
        c = scancode_shift_table[code];
    else
        c = scancode_table[code];

    return c;
}
char read_char() {
    uint8_t scancode;

    // Ждём, пока в буфере появятся данные
    while (!(inb(0x64) & 1));

    scancode = inb(0x60);

    // Игнорируем отпускание клавиши (бит 7)
    if (scancode & 0x80) {
        return 0; // или можно зациклить ожидание
    }

    char c = process_scancode(scancode);

    timer_sleep(10); // небольшая пауза, если нужно

    return c;
}

#endif