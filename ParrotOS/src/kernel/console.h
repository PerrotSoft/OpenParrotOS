#include <stdint.h>
volatile char* vga_buffer = (volatile char*)0xB8000;
static uint16_t vga_position = 0;

#define VGA_COLOR 0x07

// Чтение порта (x86)
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Печать строки
void print(const char* str) {
    while (*str) {
        if (*str == '\n') {
            vga_position += 80 - (vga_position % 80);
        } else {
            vga_buffer[vga_position * 2] = *str;
            vga_buffer[vga_position * 2 + 1] = VGA_COLOR;
            vga_position++;
        }
        str++;
    }
}

// Печать одного символа
void print_char(char c) {
    if (c == '\n') {
        vga_position += 80 - (vga_position % 80);
    } else {
        vga_buffer[vga_position * 2] = c;
        vga_buffer[vga_position * 2 + 1] = VGA_COLOR;
        vga_position++;
    }
}

// Чтение с клавиатуры (через порт 0x60)
char read_char() {
    static const char scancode_table[] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
        'a','s','d','f','g','h','j','k','l',';','\'','`',0,
        '\\','z','x','c','v','b','n','m',',','.','/',0,
        '*',0,' ', // и т.д., по необходимости дополни
    };

    uint8_t scancode = 0;
    do {
        scancode = inb(0x60);
    } while (scancode & 0x80);  // Ждём нажатие (бит 7 == 0)

    if (scancode < sizeof(scancode_table))
        return scancode_table[scancode];
    else
        return 0;
}

// Чтение строки
void read_line(char* buffer, uint16_t max_len) {
    uint16_t i = 0;
    while (i < max_len - 1) {
        char c = read_char();
        if (c == '\n' || c == '\r') break;
        if (c == '\b' && i > 0) {
            i--;
            print_char('\b');
        } else if (c >= 32 && c <= 126) {
            buffer[i++] = c;
            print_char(c);
        }
    }
    buffer[i] = 0;
}
