#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "console.h"
#include <stdlib.h>
#include "sys.h"
#include "ramfs.h"
#include <stdio.h>
#define Max_Users 50
void set_graphics_mode_300x200_32bpp(void);
typedef struct {
    char* user_name;
    char user_type[4]; // "Sys", "Usr", etc.
    char* password;
} User;

User user[Max_Users];
int enduser = 0;


// Текущий пользователь
char* user_name="null";
char user_type[4]="Sys";

char* strcpy(char* dst, const char* src) {
    char* r = dst;
    while ((*dst++ = *src++));
    return r;
}


// strncmp
int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c1 = (unsigned char)s1[i];
        unsigned char c2 = (unsigned char)s2[i];
        if (c1 != c2) return c1 - c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

// strncpy
char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}
void uint8_to_hex(uint8_t byte, char out[3]) {
    const char* hex_chars = "0123456789ABCDEF";
    out[0] = hex_chars[(byte >> 4) & 0x0F]; // старший nibble
    out[1] = hex_chars[byte & 0x0F];        // младший nibble
    out[2] = '\0';
}
// memcpy
void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}
// strcmp
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Копирует строку от позиции sti до конца, выделяет память
char* st(const char* text, int sti) {
    int len = strlen(text + sti);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    for (int i = 0; i < len; ++i) result[i] = text[sti + i];
    result[len] = '\0';
    return result;
}

// Разделяет строку text по символу s
char** str(const char* text, char s) {
    int count = 1;
    for (int i = 0; text[i]; ++i)
        if (text[i] == s) count++;
    char** result = (char**)malloc((count + 1) * sizeof(char*));
    if (!result) return NULL;
    int start = 0, part = 0;
    for (int i = 0;; ++i) {
        if (text[i] == s || text[i] == '\0') {
            int len = i - start;
            result[part] = (char*)malloc(len + 1);
            if (!result[part]) {
                for (int j = 0; j < part; j++) free(result[j]);
                free(result);
                return NULL;
            }
            strncpy(result[part], &text[start], len);
            result[part][len] = '\0';
            part++;
            start = i + 1;
        }
        if (text[i] == '\0') break;
    }
    result[part] = NULL;
    return result;
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

// Разбор двух чисел из строки вида "x,y"
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
    print_char('\n');
}
void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) {
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    } else {
        for (size_t i = n; i != 0; i--)
            d[i-1] = s[i-1];
    }
    return dest;
}
void* memset(void* dest, int val, unsigned int len) {
    unsigned char* ptr = dest;
    while (len-- > 0)
        *ptr++ = (unsigned char)val;
    return dest;
}
// Формируем строку "<число> bytes" в буфер
void format_size_str(uint32_t size, char* buf, size_t buf_size) {
    // чисто пример — вызов print_to_buf
    print_to_buf(buf, buf_size, "%u bytes", size);
}
int parse_fname_offset(const char *cmd, char *fname, uint32_t *offset) {
    const char *p = cmd;
    char *fptr = fname;

    while (*p && *p != ' ' && (fptr - fname) < 31) {
        *fptr++ = *p++;
    }
    *fptr = '\0';

    while (*p == ' ') p++;

    if (*p) {
        uint32_t val = 0;
        int digit_found = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            p++;
            digit_found = 1;
        }
        if (digit_found) {
            *offset = val;
            return 2;
        } else {
            return 1;
        }
    }

    return 1;
}

int parse_ramfs_create(const char* cmd, char* out_name, uint32_t* out_size) {
    // cmd указывает на строку после "ramfs-create "
    // читаем имя (до пробела) и число (size)

    const char* p = cmd;
    int i = 0;

    // Считаем имя
    while (*p && *p != ' ' && i < RAMFS_MAX_NAME_LEN - 1) {
        out_name[i++] = *p++;
    }
    out_name[i] = '\0';

    if (*p != ' ') return 0;
    p++;

    // Читаем число
    uint32_t val = 0;
    if (*p == '\0') return 0;
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }

    if (*p != '\0' && *p != '\n') return 0; // лишние символы - ошибка

    *out_size = val;
    return 1;
}

void print_file(const ramfs_file_t* file) {
            print(file->name);
            print(" (");
            char buf[20];
            print_to_buf(buf, sizeof(buf), "%u bytes", file->size);
            print(buf);
            print(")\n");
        }
        extern void int_input_and_output_sys();
void PCode() {
}
void run_command(char command[255]) {
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
        print("  sudo name       - Run Soft\n");
        print("  add-soft name   - Add Soft\n");
        print("  ramfs-reinit    - reinit RamFS\n");
        print("  rcf name        - Create File to RamFS\n");
        print("  rrf name        - Read File to RamFS\n");
        print("  rwf name,data   - Write File to RamFS\n");
        print("  rls             - print list Files RamFS\n");
    } else if (strcmp(command, "get-sys") == 0) {
        char** specs = NULL;
        get_PC_specifications(&specs);
        if (specs) {
            for (int i = 0; specs[i] != NULL; i++) {
                print(specs[i]);
                print_char('\n');
            }
        } else {
            print("Unable to get system specs.\n");
        }
    } else if (strncmp(command, "color ", 6) == 0) {
        char* params = st(command, 6);
        if (params != NULL) {
            int tc, bc;
            if (parse_two_numbers(params, &tc, &bc)) {
                VGA_COLOR = ((bc & 0x0F) << 4) | (tc & 0x0F);
                print("Color set.\n");
            } else {
                print("Invalid color format. Use: color tc,bc\n");
            }
            free(params);
        }
    } else if (strcmp(command, "clr") == 0 || strcmp(command, "clear") == 0) {
        fill_screen((uint8_t)VGA_COLOR);
    } else if (strncmp(command, "set-cursor ", 11) == 0) {
        char* params = st(command, 11);
        if (params != NULL) {
            int x, y;
            if (parse_two_numbers(params, &x, &y)) {
                set_cursor((uint8_t)x, (uint8_t)y);
                print("Cursor position set.\n");
            } else {
                print("Invalid set-cursor format. Use: set-cursor x,y\n");
            }
            free(params);
        }
    } else if (strncmp(command, "print ", 6) == 0) {
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
    }else if (strncmp(command, "sudo ",5) == 0) {
        char* text = st(command,5);
        uint8_t* code;
        int res = ramfs_read(text, code, 2048, 0);
        if (res < 0) {
            print("Error reading file\n");
        } else {
            print_char(run_soft(code));
        }
    }else if (strcmp(command, "pcode") == 0) {
        PCode();
    }else if (strcmp(command, "ramfs-reinit") == 0) {
        ramfs_init();
        print("RAMFS ReInitialized\n");
    }else if (strncmp(command, "rcf ", 4) == 0) {
        char* name = st(command, 4);
        int res = ramfs_create(name, 2048);
        if (res == 0) {
            print("File created successfully\n");
        } else {
            print("Error creating file\n");
        }
    }else if (strncmp(command, "rrf ", 4) == 0) {
        char* name = st(command, 4);
        static char buf[2049]; // +1 для '\0'
        int res = ramfs_read(name, buf, 2048, 0);
        if (res < 0) {
            print("Error reading file\n");
        } else {
            buf[res] = '\0'; // Чтобы можно было печатать как строку
            print(buf);
            print("\n");
        }
    }
    else if (strncmp(command, "rwf ", 4) == 0) {
        char** parts = str(st(command, 4), ',');
        char* name = parts[0];
        char* data = parts[1];
        int res = ramfs_write(name, data, strlen(data), 0);
        if (res == 0) {
            print("Data written successfully\n");
        } else {
            print("Error writing to file\n");
        }
    }
    else if (strcmp(command, "rls") == 0) 
        ramfs_list(print_file);
    else if (strncmp(command, "rdf ", 4) == 0) {
        char* name = st(command, 4);
        int res = ramfs_delete(name);
        if (res == 0) {
            print("File deleted successfully\n");  // исправлено сообщение
        } else {
            print("Error deleting file\n");        // исправлено сообщение
        }
    }else if (strncmp(command, "ref ", 4) == 0) {
        char* name = st(command, 4);
        int res = ramfs_find_file_index(name);
        if (res == -1) {
            print("Error: file not found\n");    // исправлено сообщение
        } else {
            print("File found successfully\n");  // исправлено сообщение
        }
    }else if (strncmp(command, "rrbf ", 5) == 0) {
        char* name = st(command, 5);
        static uint8_t buf[2048];
        int res = ramfs_read(name, buf, sizeof(buf), 0);
        if (res < 0) {
            print("Error reading file\n");
        } else {
            char hex_str[3];
            for (int i = 0; i < res; i++) {
                uint8_to_hex(buf[i], hex_str);
                print(hex_str);
                print(" ");
            }
            print("\n");
        }
    }else if (strncmp(command, "rwbf ", 5) == 0) {
    char** parts = str(st(command, 5), ',');
    char* name = parts[0];
    char* hex_data = parts[1];

    static uint8_t code_bytes[500]; // 2 hex символа = 1 байт

    // Конвертируем hex-строку в байты
    int code_len = hex_string_to_bytes(hex_data, sizeof(code_bytes), code_bytes);
    if (code_len < 0) {
        print("Error: invalid hex string\n");
        free(parts);
        return;
    }

    // Записываем в файл реальные байты
    int res = ramfs_write(name, code_bytes, code_len, 0);
    if (res == 0) {
        print("Data written successfully\n");
    } else {
        print("Error writing to file\n");
    }

    free(parts);
}
else {
        print("Unknown command. Type 'help' for a list of commands.\n");
    }
}


// Главный цикл командной оболочки
void main_cmd() {
    ramfs_init();
    char command[255];
    while (true) {
        print(user_name);
        print_char('~');
        print(user_type);
        print(":> ");
        
        read_Shell(command, sizeof(command));

        print_char('\n');

        if (strcmp(command, "exit") == 0) {
            print("Exit command received. Halting.\n");
            break;
        }

        run_command(command);
    }
}
