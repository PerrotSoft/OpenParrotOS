#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h> 
#include "io.h" // чтобы inb/outb были известны

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_CAPSLOCK 0x3A
#define SC_F1 0x45  // условное обозначение
#define SC_F2 0x46  // условное обозначение
#define SC_F3 0x47  // условное обозначение
#define SC_F4 0x47  // условное обозначение
#define SC_F5 0x48  // условное обозначение
#define SC_F6 0x49   // условное обозначение
#define SC_F7 0x4A // условное обозначение
#define SC_F8 0x4B  // условное обозначение
#define SC_F9 0x4C  // условное обозначение
#define SC_F10 0x4D  // условное обозначение
#define SC_UP 0x3A // условное обозначение
#define SC_Down 0x3B  // условное обозначение
#define SC_R 0x3C  // условное обозначение
#define SC_L 0x3D  // условное обозначение
#define SC_PUP 0x3E // условное обозначение
#define SC_PDown 0x3F  // условное обозначение
#define SC_ESC 27  // условное обозначение
volatile char* vga_buffer = (volatile char*)0xB8000;
static uint16_t vga_position = 0;

int VGA_COLOR = 0x0F;
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define HISTORY_SIZE 9999
#define MAX_COMMAND_LENGTH 255

static bool shift_pressed = false;
static bool caps_lock = false;

// Внешние функции для вывода и управления экраном (если есть, можно заменить)
void set_cursor(int row, int col);
void fill_screen(int color);


char* int_to_str(int num) {
    char* str;
    int i = 0;
    bool is_negative = false;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (num < 0) {
        is_negative = true;
        num = -num;
    }

    // Записываем цифры в обратном порядке
    while (num != 0) {
        int rem = num % 10;
        str[i++] = (char)(rem + '0');
        num /= 10;
    }

    if (is_negative) {
        str[i++] = '-';
    }

    str[i] = '\0';

    // Переворачиваем строку
    for (int j = 0; j < i / 2; j++) {
        char tmp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = tmp;
    }

    return str;
}
void scroll_if_needed() {
    if (vga_position / VGA_WIDTH >= VGA_HEIGHT) {
        for (int y = 1; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[(y - 1) * VGA_WIDTH * 2 + x * 2] = vga_buffer[y * VGA_WIDTH * 2 + x * 2];
                vga_buffer[(y - 1) * VGA_WIDTH * 2 + x * 2 + 1] = vga_buffer[y * VGA_WIDTH * 2 + x * 2 + 1];
            }
        }
        // Очистить последнюю строку
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH * 2 + x * 2] = ' ';
            vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH * 2 + x * 2 + 1] = VGA_COLOR;
        }

        vga_position -= VGA_WIDTH;
    }
}

void print_char(char c) {
    scroll_if_needed();
    if (c == '\n') {
        vga_position += VGA_WIDTH - (vga_position % VGA_WIDTH);
    } else if (c == '\b') {
        if (vga_position > 0) {
            vga_position--;
            vga_buffer[vga_position * 2] = ' ';
            vga_buffer[vga_position * 2 + 1] = VGA_COLOR;
        }
    } else {
        vga_buffer[vga_position * 2] = c;
        vga_buffer[vga_position * 2 + 1] = VGA_COLOR;
        vga_position++;
    }
}

void print(const char* str) {
    while (*str) {
        print_char(*str++);
    }
}

void print_int(const int stri) {
    char* str =int_to_str(stri);
    while (*str) {
        print_char(*str++);
    }
}
void print_bytes(const uint8_t stri) {
    char* str =int_to_str(stri);
    while (*str) {
        print_char(*str++);
    }
}
void fill_screen(int color) {
    if (color == -1)
        color = VGA_COLOR;
    int old_color = VGA_COLOR;
    VGA_COLOR = color;

    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i * 2] = ' ';
        vga_buffer[i * 2 + 1] = (uint8_t)VGA_COLOR;
    }

    vga_position = 0;
    VGA_COLOR = old_color;
}

void set_cursor(int row, int col) {
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;

    vga_position = row * VGA_WIDTH + col;
}

static const char scancode_table[] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,
    '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',18,' ',0,SC_F1,SC_F2,SC_F3,SC_F4,SC_F5,SC_F6,SC_F7,SC_F8,SC_F9,SC_F10
    ,0,0,0,SC_UP,SC_PUP,0,SC_L,0,SC_R,0,0,SC_Down
};

static const char scancode_shift_table[] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,
    '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',18,' ',0,SC_F1,SC_F2,SC_F3,SC_F4,SC_F5,SC_F6,SC_F7,SC_F8,SC_F9,SC_F10
    ,0,0,0,SC_UP,SC_PUP,0,SC_L,0,SC_R,0,0,SC_Down
};

// Убери дублирующее объявление
// static bool shift_pressed = false;  <- убираем
// static bool caps_lock = false;      <- убираем

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
    uint8_t scancode = 0;

    // Ждем нажатия (сканкод с битом отпускания = 0)
    do {
        scancode = inb(0x60);
    } while (scancode & 0x80); 

    char c = process_scancode(scancode);
    // Ждем отпускания (сканкод с битом отпускания = 1)
    do {
        scancode = inb(0x60);
    } while (!(scancode & 0x80));

    return c;
}
void read_line(char* buffer, uint16_t max_len) {
    uint16_t i = 0;
    while (i < max_len - 1) {
        char c = read_char();

        if (c == '\n' || c == '\r')
            break;
        else if (c == '\b' && i > 0) {
            i--;
            print_char('\b');
        } else if (c == '\t') {
            for (int t = 0; t < 4 && i < max_len - 1; t++) {
                buffer[i++] = ' ';
                print_char(' ');
            }
        } else if (c >= 32 && c <= 126) {
            buffer[i++] = c;
            print_char(c);
        }
    }
    buffer[i] = '\0';
}


void read_pass(char* buffer, uint16_t max_len) {
    uint16_t i = 0;
    while (i < max_len - 1) {
        char c = read_char();

        if (c == '\n' || c == '\r')
            break;
        else if (c == '\b' && i > 0) {
            i--;
            print_char('\b');
        }else if (c >= 32 && c <= 126) {
            buffer[i++] = c;
            print_char('*');
        }
    }
    buffer[i] = '\0';
}


char history[HISTORY_SIZE][MAX_COMMAND_LENGTH];
int history_count = 0;
int history_index = -1;

void read_Shell(char* buffer, uint16_t max_len) {
    uint16_t i = 0;
    buffer[0] = '\0';
    int navigating_history = 0;

    while (i < max_len - 1) {
        char c = read_char();

        if (c == '\n' || c == '\r') {
            break;
        } else if (c == '\b' && i > 0) {
            i--;
            print_char('\b');
            print_char(' ');
            print_char('\b');
        } else if (c == SC_UP) {
            if (history_count > 0 && history_index + 1 < history_count) {
                history_index++;
                // Стереть текущую строку
                while (i > 0) {
                    print_char('\b');
                    print_char(' ');
                    print_char('\b');
                    i--;
                }
                // Скопировать строку из истории
                strcpy(buffer, history[history_count - 1 - history_index]);
                i = strlen(buffer);
                print(buffer);
            }
        } else if (c == SC_Down) {
            if (history_index > 0) {
                history_index--;
            } else {
                history_index = -1;
            }

            // Стереть текущую строку
            while (i > 0) {
                print_char('\b');
                print_char(' ');
                print_char('\b');
                i--;
            }

            if (history_index >= 0) {
                strcpy(buffer, history[history_count - 1 - history_index]);
                i = strlen(buffer);
                print(buffer);
            } else {
                buffer[0] = '\0';
                i = 0;
            }
        } else if (c == '\t') {
            for (int t = 0; t < 4 && i < max_len - 1; t++) {
                buffer[i++] = ' ';
                print_char(' ');
            }
        } else if (c >= 32 && c <= 126) {
            buffer[i++] = c;
            print_char(c);
        }
    }

    buffer[i] = '\0';

    // Добавляем в историю, если строка не пустая
    if (i > 0) {
        if (history_count < HISTORY_SIZE) {
            strcpy(history[history_count++], buffer);
        } else {
            // Сдвигаем историю вверх
            for (int j = 1; j < HISTORY_SIZE; j++) {
                strcpy(history[j - 1], history[j]);
            }
            strcpy(history[HISTORY_SIZE - 1], buffer);
        }
    }

    history_index = -1;
}
void cube(int row, int col,int SizeX,int SizeY,int color){
    SizeX--;
    SizeY--;
    if (color == -1)
        color = VGA_COLOR;
    int old_color = VGA_COLOR;
    VGA_COLOR = color;
    for (int i = 0; i <= SizeY; i++)
    for (int j = 0; j <= SizeX; j++)
    {
        set_cursor(row+i,col+j);
        print_char(' ');
    }
    VGA_COLOR = old_color;
}
#endif