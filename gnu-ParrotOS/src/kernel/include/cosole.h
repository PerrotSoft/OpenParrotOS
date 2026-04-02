#include <uchar.h>
#include <stdint.h>
#include "drivers/video_driver.h"
#include "io.h"
#include "drivers/keyboard_driver.h"
#include "sys.h"
#include "fs.h"
#define Max_Users 50
int x=0;
int y=0;

typedef struct {
    char* user_name;
    char user_type[4]; // "Sys", "Usr", etc.
    char* password;
} User;

User user[Max_Users];
int enduser = 0;
uint8_t VGA_COLOR = 0xF;

uint32_t vga_to_x32(uint8_t vga_color) {
    switch (vga_color & 0x0F) {
        case 0x0: return Black;
        case 0x1: return Blue;
        case 0x2: return Green;
        case 0x3: return Cyan;
        case 0x4: return Red;
        case 0x5: return Magenta;
        case 0x6: return Brown;
        //case 0x7: return LightGray;
        //case 0x8: return DarkGray;
        case 0x9: return LightBlue;
        case 0xA: return LightGreen;
        //case 0xB: return LightCyan;
        //case 0xC: return LightRed;
        //case 0xD: return LightMagenta;
        case 0xE: return Yellow;
        case 0xF: return White;
        default:  return Black;
    }
}

void print(const char* str) {
    while (*str) {
        print_char_xy(x*(CHAR_W + CHAR_SPACING), y*(CHAR_H + (CHAR_SPACING*2)), *str++, vga_to_x32(VGA_COLOR));
        if(*str=='\n'){
            y++;
            x=0;
        }
        else
            x++;
    }
}
void print_char(const char str) {
        print_char_xy(x*(CHAR_W + CHAR_SPACING), y*(CHAR_H + (CHAR_SPACING*2)), str, White);
        if(str=='\n'){
            y++;
            x=0;
        }
        else
            x++;
}

void read_line(char* buffer, uint16_t max_len) {
    uint16_t i = 0;
    while (i < max_len - 1) {
        char c = read_char();
        if (c != '\0') {
            if (c == '\n' || c == '\r')
                break;
            else if (c == '\b' && i > 0) {
                i--;
                x--;
                print_char_xy(x*(CHAR_W + CHAR_SPACING), y*(CHAR_H + CHAR_SPACING), buffer[i], Black);
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
    }
    buffer[i] = '\0';
}


void read_pass(char* buffer, uint16_t max_len) {
    uint16_t i = 0;
    while (i < max_len - 1) {
        char c = read_char();
        if (c != '\0') {
            if (c == '\n' || c == '\r')
                break;
            else if (c == '\b' && i > 0) {
                i--;
                x--;
                print_char_xy(x*(CHAR_W + CHAR_SPACING), y*(CHAR_H + CHAR_SPACING), buffer[i], Black);
            }else if (c >= 32 && c <= 126) {
                buffer[i++] = c;
                print_char('*');
            }
        }
    }
    buffer[i] = '\0';
}
char* int_to_string(int value) {
    char* buffer = (char*)malloc(12); // -2147483648 + '\0'
    if (!buffer) return NULL;

    int i = 0;
    int negative = 0;

    if (value < 0) {
        negative = 1;
        value = -value;
    }

    if (value == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return buffer;
    }

    int temp = value;
    while (temp > 0) {
        buffer[i++] = '0' + (temp % 10);
        temp /= 10;
    }

    if (negative) {
        buffer[i++] = '-';
    }

    buffer[i] = '\0';

    // Переворачиваем
    for (int j = 0; j < i / 2; j++) {
        char t = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = t;
    }

    return buffer;
}
void print_int(int i) {
    char* str = int_to_string(i);
    while (*str) {
        print_char_xy(x*(CHAR_W + CHAR_SPACING), y*(CHAR_H + (CHAR_SPACING*2)), *str++, White);
        if(*str=='\n'){
            y++;
            x=0;
        }else
            x++;
    }
}
// Текущий пользователь
char* user_name="null";
char user_type[4]="Sys";
char* strcat(char* dest, const char* src) {
    char* d = dest;
    while (*d) d++;           // идём в конец строки
    while ((*d++ = *src++));  // копируем src включая '\0'
    return dest;
}


// Загрузка пользователя
bool load_user(const char* uname, const char* pass) {
    for (int i = 0; i < enduser; i++) {
        if (strcmp(user[i].user_name, uname) == 0 &&
            strcmp(user[i].password, pass) == 0) {
            user_name = user[i].user_name;
            strncpy(user_type, user[i].user_type, 4);
            return true;
        }
    }
    return false;
}

void add_user(const char* uname, const char* pass, const char type[4]) {
    if (enduser >= Max_Users) {
        print("User limit reached.\n");
        return;
    }

    user[enduser].user_name = (char*)malloc(strlen(uname) + 1);
    strcpy(user[enduser].user_name, uname);

    user[enduser].password = (char*)malloc(strlen(pass) + 1);
    strcpy(user[enduser].password, pass);

    strncpy(user[enduser].user_type, type, 4);
    enduser++;

    print("User added successfully.\n");
    load_user(uname,pass);
    print("remaining number of users: z");
    print_int(Max_Users-enduser);
}
// Освобождение памяти для массива строк
void free_str_array(char** arr) {
    if (!arr) return;
    for (int i = 0; arr[i] != NULL; i++) free(arr[i]);
    free(arr);
}
// --- Простые аналоги isspace и isdigit ---

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

static bool is_digit(char c) {
    return (c >= '0' && c <= '9');
}

// Пропустить пробелы
static const char* skip_spaces(const char* str) {
    while (*str && is_space(*str)) str++;
    return str;
}

long strtol(const char* str, char** endptr, int base) {
    long result = 0;
    int sign = 1;

    // Пропустить пробелы
    while (*str == ' ' || *str == '\t') str++;

    // Знак
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    // Определить base (если base == 0)
    if (base == 0) {
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
            base = 16;
            str += 2;
        } else if (str[0] == '0') {
            base = 8;
            str++;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
            str += 2;
        }
    }

    // Основной цикл
    while (*str) {
        int digit;
        if (*str >= '0' && *str <= '9') digit = *str - '0';
        else if (*str >= 'a' && *str <= 'f') digit = *str - 'a' + 10;
        else if (*str >= 'A' && *str <= 'F') digit = *str - 'A' + 10;
        else break;

        if (digit >= base) break;

        result = result * base + digit;
        str++;
    }

    if (endptr) *endptr = (char*)str;
    return result * sign;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack; // пустая строка совпадает сразу

    for (const char* p = haystack; *p; p++) {
        const char* h = p;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char*)p; // нашли совпадение
    }
    return 0; // не нашли
}
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Парсит строку с hex-цифрами в буфер. Игнорирует пробелы, запятые и префиксы 0x/0X.
// Возвращает число записанных байт. Odd nibble -> последний байт будет 0x0N.
static uint32_t parse_hex_stream(const char* s, uint8_t** out_buf) {
    // сначала посчитаем количество нибблов (hex-цифр)
    uint32_t nibbles = 0;
    for (const char* p = s; *p; ++p) {
        if ((p[0] == '0') && (p[1] == 'x' || p[1] == 'X')) { p++; continue; }
        if (hex_val(*p) >= 0) nibbles++;
    }
    if (nibbles == 0) { *out_buf = 0; return 0; }

    uint32_t bytes = (nibbles + 1) / 2;
    uint8_t* buf = (uint8_t*)malloc(bytes);
    if (!buf) { *out_buf = 0; return 0; }

    int hi = -1;
    uint32_t count = 0;
    for (const char* p = s; *p && count < bytes; ++p) {
        int v = -1;
        if ((p[0] == '0') && (p[1] == 'x' || p[1] == 'X')) { p++; continue; }
        v = hex_val(*p);
        if (v < 0) {
            // игнорируем любые не-hex символы (пробелы, запятые и т.д.)
            continue;
        }
        if (hi < 0) {
            hi = v; // старший ниббл
        } else {
            buf[count++] = (uint8_t)((hi << 4) | v);
            hi = -1;
        }
    }
    if (hi >= 0 && count < bytes) {
        // остался один ниббл — трактуем как 0x0N
        buf[count++] = (uint8_t)hi;
    }

    *out_buf = buf;
    return count;
}
bool parse_two_numbers(const char* str, int* x, int* y) {
    if (!str || !x || !y) return false;
    str = skip_spaces(str);
    if (!is_digit(*str)) return false;
    int val1 = 0;
    while (is_digit(*str)) {
        val1 = val1 * 10 + (*str - '0');
        str++;
    }
    str = skip_spaces(str);
    if (*str != ',') return false;
    str++;
    str = skip_spaces(str);
    if (!is_digit(*str)) return false;
    int val2 = 0;
    while (is_digit(*str)) {
        val2 = val2 * 10 + (*str - '0');
        str++;
    }
    str = skip_spaces(str);
    if (*str != '\0') return false;
    *x = val1;
    *y = val2;
    return true;
}

void run_command(char command[174]){
    if (strcmp(command, "shutdown") == 0) {
        print("Shutting down...\n");
        shutdown();
    } else if (strcmp(command, "reboot") == 0) {
        print("Rebooting...\n");
        reboot();
    } else if (strcmp(command, "help") == 0) {
        print("Available commands:\n");
        print("  help            - Show this help message\n");
        print("  shutdown        - Power off the system\n");
        print("  reboot          - Reboot the system\n");
        print("  exit            - Exit the shell\n");
        print("  clr             - Clear the shell screen\n");
        print("  color tc,bc     - Set text and background colors\n");
        print("  get-sys         - Show system specifications\n");
        print("  set-cursor x,y  - Set cursor position\n");
        print("  login           - Login user\n");
        print("  regist          - Regist user\n");
        print("  cf name         - Create file\n");
        print("  cat name        - Read file\n");
        print("  wf name,data    - Write file\n");
        print("  sudo name       - Run soft on file\n");
        print("  cwf name,data   - Create file\n");
        print("  catb name       - Read Byte file\n");
        print("  wfb name        - Write Byte file\n");
        print("  cp name,name2   - Copy file\n");
        print("  rn name,name    - Readname filen");
        print("  rf name         - Read file\n");
        print("  del name        - Delate file\n");
        print("  ls              - List files file\n");
        print("  open name       - Open file\n");
    }else if (strcmp(command, "clr") == 0 || strcmp(command, "clear") == 0) {
        clear_screen(Black);
        y=0;
    } else if (strncmp(command, "color ", 6) == 0) {
        char* params = st(command, 6);
        if (params != NULL) {
            int tc, bc;
            if (parse_two_numbers(params, &tc, &bc)) {
                VGA_COLOR = ((tc & 0x0F) << 4);
            } else {
                print("Invalid color format. Use: color tc,bc\n");
            }
            free(params);
        }
    } else if (strncmp(command, "set-cursor ", 11) == 0) {
        char* params = st(command, 11);
        if (params != NULL) {
            int x1, y1;
            if (parse_two_numbers(params, &x1, &y1)) {
                x=x1;y=y1;
                print("Cursor position set.\n");
            } else {
                print("Invalid set-cursor format. Use: set-cursor x,y\n");
            }
            free(params);
        }
    }else if (strncmp(command, "print ", 6) == 0) {
        char* params = st(command, 6);
        if (params != NULL) {
            print(params);
            print_char('\n');
            free(params);
        }
    }else if (strcmp(command, "regist") == 0) {
        char name[100];
        char pass[100];

        print("user Name: ");
        read_line(name, sizeof(name));

        print("\nuser Password: ");
        read_pass(pass, sizeof(pass));
        print_char('\n');
        add_user(name, pass, "Usr");  // или "Sys", "Root" и т.д.
    }else if (strcmp(command, "login") == 0) {
        char name[100];
        char pass[100];

        print("user Name: ");
        read_line(name, sizeof(name));

        print("\nuser Password: ");
        read_pass(pass, sizeof(pass));
        print_char('\n');
        load_user(name, pass);  // или "Sys", "Root" и т.д.
    }else if (strncmp(command, "cf ", 3) == 0) {
        char* name = st(command, 3);
        uint8_t data[] = {0xb8, 0x68, 0x00, 0x00, 0x00, 0xC3};
        create_file(name, 1, data);

    } else if (strncmp(command, "cwf ", 4) == 0) {
        char** name = str(st(command, 4), ',');
        create_file(name[0], 1, name[1]);

    } else if (strncmp(command, "rf ", 3) == 0) {
        char* name = st(command, 3);
        struct fs_files file = read_file(name);
        if (file.data) {
            file.data[file.size * 512 - FILE_HEADER_SIZE] = '\0';
            print((char*)file.data);
            print("\n");
            free(file.data);
        } else {
            print("Error: file not found\n");
        }

    } else if (strncmp(command, "wf ", 3) == 0) {
        char** name = str(st(command, 3), ',');
        write_file(name[0], (uint8_t*)name[1], strlen(name[1]));

     } else if (strncmp(command, "del ", 4) == 0) {
        char* name = st(command, 4);
        int res = delete_file(name);
        if (res == 0) print("File deleted successfully\n");
        else print("Error: file not found\n");

    } else if (strncmp(command, "rn ", 3) == 0) {
        char** names = str(st(command, 3), ',');
        int res = rename_file(names[0], names[1]);
        if (res == 0) print("File renamed successfully\n");
        else print("Error: file not found\n");

    } else if (strncmp(command, "cp ", 3) == 0) {
        char** names = str(st(command, 3), ',');
        if (copy_file(names[0], names[1])) print("File copied successfully\n");
        else print("Error: file not found or copy failed\n");

    } else if (strncmp(command, "exists ", 7) == 0) {
        char* name = st(command, 7);
        if (exen_file(name)) print("File exists\n");
        else print("File does not exist\n");

    }else if (strncmp(command, "catb ", 5) == 0) {
    char* name = st(command, 5);
    struct fs_files file = read_file(name);

    if (!file.data) {
        print("Error: file not found\n");
    } else {
        uint32_t total_bytes = file.size * 512 - FILE_HEADER_SIZE;
        uint8_t* data = file.data;

        // Настройки экрана
        uint32_t max_width = 50;
        uint32_t max_height = 40;

        // Если нужно — увеличиваем
        if (total_bytes > max_width * max_height) max_width = 100;
        if (total_bytes > max_width * max_height) max_height = 50;

        uint32_t bytes_per_line = max_width;
        uint32_t lines = (total_bytes + bytes_per_line - 1) / bytes_per_line;

        for (uint32_t line = 0; line < lines; line++) {
            uint32_t start = line * bytes_per_line;
            uint32_t end = start + bytes_per_line;
            if (end > total_bytes) end = total_bytes;

            // HEX-колонка
            for (uint32_t i = start; i < end; i++) {
                uint8_t b = data[i];
                char hex[3];
                hex[0] = (b >> 4) < 10 ? '0' + (b >> 4) : 'A' + ((b >> 4) - 10);
                hex[1] = (b & 0xF) < 10 ? '0' + (b & 0xF) : 'A' + ((b & 0xF) - 10);
                hex[2] = '\0';
                print(hex);
                print(" ");
            }

            // Добавляем отступ между колонками
            print(" | ");

            // ASCII-колонка
            for (uint32_t i = start; i < end; i++) {
                char c = data[i];
                if (c >= 32 && c <= 126) print_char(c); // печатаемые символы
                else print_char('.');                   // непечатаемые → точка
            }

            print("\n");
        }

        free(file.data);
    }
}
else if (strncmp(command, "open ", 5) == 0) {
        char* name = st(command, 5);
        uint8_t* data = open_file(name);  // использует open_file для определения типа и запуска
        if (data==1) {
            print("Error: file not found or executed\n");
        } else {
            print("File opened successfully\n");
            print((char*)data);
            print("\n");
        }
    }else if (strncmp(command, "sudo ", 5) == 0) {
        char* name = st(command, 5);
        
        struct file_type f = open_to_type(name);

        if (f.type == file_sys || f.type == file_bin) {
            // если это исполняемый бинарь
            char result = execute_binary_rchar(f.file.data);
            print_char(result);
            print_char('\n');
        } else {
            print("Error: not an executable file\n");
        }
    }
    else if (strcmp(command, "ls") == 0) {
        for (size_t i = 0; i < current_file_count; i++)
            if (t_fs[i].name && t_fs[i].name[0] != '\0') {
                print(t_fs[i].name);
                print_char('\n');
            }
        print_char('\n');

    } else if (strcmp(command, "clear-fs") == 0) {
        clear_drive();
    }else if (strncmp(command, "wbf ", 4) == 0) {
    char** parts = str(st(command, 4), ','); // parts[0] = имя файла (может быть с пробелами), parts[1] = hex-строка
    char* filename = parts[0];
    char* hexstream = parts[1];

    if (!filename || !hexstream) {
        print("WBF syntax: wbf <file>,<hex>\n");
    } else {
        uint8_t* data = 0;
        uint32_t len = parse_hex_stream(hexstream, &data);
        if (!data || len == 0) {
            print("WBF error: no hex data\n");
        } else {
            // Посчитаем, сколько секторов нужно для этих len байт
            uint32_t need_sectors = (len + FILE_HEADER_SIZE + 511) / 512;

            // Проверим, есть ли уже файл и какая у него ёмкость
            struct fs_files existing = read_file(filename); // existing.size = кол-во секторов
            if (!existing.data) {
                // файла нет — создаём с нужным количеством секторов
                create_file(filename, need_sectors ? need_sectors : 1, data);
                print("WBF: file created\n");
            } else {
                uint32_t old_sectors = existing.size ? existing.size : 1;
                free(existing.data);
                if (need_sectors <= old_sectors) {
                    // влезает — просто перезаписываем
                    int wr = write_file(filename, data, len);
                    if (wr == 0) {
                        print("WBF: file updated\n");
                    } else {
                        print("WBF write error\n");
                    }
                } else {
                    // не влезает — пересоздадим файл под новый размер
                    int del = delete_file(filename);
                    (void)del;
                    create_file(filename, need_sectors, data);
                    print("WBF: file recreated (grown)\n");
                }
            }
            free(data);
        }
    }
}

else {
        print("Unknown command. Type 'help' for a list of commands.\n");
    }
}
void main_cmd() {
    init_fs(2);
    char buffer[64]; // строка для вывода
    buffer[0] = '\0';
    strcpy(buffer, user_name);
    strcat(buffer, "~");
    strcat(buffer, user_type);
    strcat(buffer, ":> ");

    char text[174];

    while (1) {
        print(buffer);
        read_line(text,174);
        print_char('\n');
        run_command(text);
    }
}
