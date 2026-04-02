#ifndef SYS_H
#define SYS_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>   /* нужен для print_to_buf */
#include "console.h"

void cpuid(uint32_t eax, uint32_t ecx, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    uint32_t eax_, ebx_, ecx_, edx_;
    __asm__ volatile (
        "cpuid"
        : "=a"(eax_), "=b"(ebx_), "=c"(ecx_), "=d"(edx_)
        : "a"(eax), "c"(ecx)
    );
    if (a) *a = eax_;
    if (b) *b = ebx_;
    if (c) *c = ecx_;
    if (d) *d = edx_;
}

uint64_t get_ram_size_bytes(void) {
    // базовый CMOS (640К) плюс 384К расширенной базовой памяти
    // Здесь можно читать CMOS 0x15 и 0x16, потом добавить 384К и/или 1MB+ если есть
    outb(0x70, 0x15);
    uint8_t low = inb(0x71);
    outb(0x70, 0x16);
    uint8_t high = inb(0x71);
    uint16_t kb = (high << 8) | low;
    if (kb == 0) kb = 640;
    // Добавим 384К расширенной базовой памяти (традиционно)
    kb += 384;
    return (uint64_t)kb * 1024 * 1024;
}

// Заглушка для занятости
unsigned long get_ram_used_bytes(void) {
    static char* fake_specs[9];
     void* ram_end = (void*)&fake_specs; // адрес массива fake_specs — ниже всех
     return (unsigned long)ram_end;
}

/* Настройка (подправь адреса/значения под ACPI твоей платформы) */
#define PM1a_CNT_BLK 0x004  /* пример, получить правильный адрес из ACPI */
#define SLP_TYP_SHIFT 10
#define SLP_EN (1U << 13)

/* Простая реализация strcmp (чтобы не подключать <string.h>) */
static int k_strcmp(const char *a, const char *b) {
    if (!a || !b) return (a == b) ? 0 : (a ? 1 : -1);
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Простая memcpy (используй свою, если есть) */
static void *k_memcpy(void *dst, const void *src, uint32_t n) {
    unsigned char *d = (unsigned char*)dst;
    const unsigned char *s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* Простая memset */
static void *k_memset(void *dst, int v, uint32_t n) {
    unsigned char *d = (unsigned char*)dst;
    while (n--) *d++ = (unsigned char)v;
    return dst;
}

void reboot() {
    __asm__ __volatile__ (
        "cli\n"                  // запрет прерываний
        "mov $0x64, %dx\n"       // порт 8042 клавиатуры
        "wait_reboot:\n"
        "inb %dx, %al\n"
        "test $0x02, %al\n"      // проверяем бит 1 (заполнен ли буфер вывода)
        "jnz wait_reboot\n"
        "mov $0xFE, %al\n"       // команда перезагрузки процессора
        "outb %al, %dx\n"
        "hlt\n"                  // если не перезагрузилась — стоп
    );
}

void fatal_error(const char *msg) {
    if (!msg) msg = "Unknown fatal error";

    VGA_COLOR = 0x1F; /* Белый текст на красном фоне */
    fill_screen(-1);

    set_cursor(2, 29);
    print(" :( Fatal Error ParrotOS");
    set_cursor(3, 29);
    print("_________________________");

    set_cursor(5, 0);
    print(msg);

    set_cursor(25, 29);
    print("Press Enter to reboot");

    for (;;) {
        char ch = read_char();

        if (ch == '\n' || ch == '\r') {
            reboot();
        } else if (ch == 0x00) {
            continue;
        } else if (ch == SC_F1) {
            return; /* попробовать вернуться */
        }
    }
}

/* ---------- hex helper ---------- */
static uint8_t hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0xFF; /* признак ошибки */
}

/* Преобразовать hex-строку в байты. Возвращает количество записанных байт, 0 при ошибке. */
uint32_t hex_string_to_bytes(const char *hex_str, uint32_t hex_str_len, uint8_t *out_bytes) {
    if (!hex_str || !out_bytes) return 0;
    if (hex_str_len % 2 != 0) return 0;

    uint32_t i = 0, j = 0;
    while (i < hex_str_len) {
        uint8_t high = hex_char_to_value(hex_str[i]);
        uint8_t low  = hex_char_to_value(hex_str[i+1]);
        if (high == 0xFF || low == 0xFF) return 0; /* неверный символ */
        out_bytes[j++] = (uint8_t)((high << 4) | low);
        i += 2;
    }
    return j;
}

int run_soft(uint8_t *code) {

        if (code == NULL) {
            fatal_error("Program code is NULL");
             return -1;
        }
        int (*func)(void) = (int(*)(void))code;

        if ((uintptr_t)func < 0x1000) {
            fatal_error("Invalid function pointer!");
        return -1;
    }
}

/* ---------- Перезагрузка и выключение ---------- */

void shutdown() {
    uint16_t val = (7 << 10) | (1 << 13);  // SLP_TYP=7 (S5), SLP_EN=1
    uint16_t port = 0x604;                  // Обычно PM1a_CNT_BLK — 0x604, может быть другим

    __asm__ __volatile__ (
        "outw %0, %1\n\t"   // Записать значение в порт
        "hlt\n"             // Остановить процессор
        :
        : "a"(val), "Nd"(port)
    );

    while(1) {
        __asm__ __volatile__("hlt");
    }
}
/* ---------- CPU / RAM helpers (прототипы) ---------- */


void cpuid(uint32_t eax, uint32_t ecx, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d);

/* ---------- Простой print_to_buf (используется вместо snprintf) ---------- */
/* Предполагаем, что uint_to_dec_str и print_to_buf используются в ядре, freestanding */
static char *uint_to_dec_str(unsigned int v, char *dst_end) {
    char *p = dst_end;
    if (v == 0) {
        *--p = '0';
        return p;
    }
    while (v) {
        *--p = '0' + (v % 10);
        v /= 10;
    }
    return p;
}

int print_to_buf(char *buf, size_t buf_size, const char *fmt, ...) {
    if (!buf || buf_size == 0) return 0;
    char *out = buf;
    char *end = buf + buf_size;
    va_list ap;
    va_start(ap, fmt);

    const char *p = fmt;
    while (*p) {
        if (*p != '%') {
            if (out + 1 >= end) break;
            *out++ = *p++;
            continue;
        }
        p++;
        if (*p == '\0') break;
        if (*p == 'd') {
            int v = va_arg(ap, int);
            unsigned int uv;
            int neg = 0;
            if (v < 0) { neg = 1; uv = (unsigned int)(-v); } else uv = (unsigned int)v;
            char tmp[12];
            char *tend = tmp + sizeof(tmp);
            char *tstart = uint_to_dec_str(uv, tend);
            if (neg) {
                if (out + 1 >= end) break;
                *out++ = '-';
            }
            size_t len = (size_t)(tend - tstart);
            if (out + len >= end) { len = (size_t)(end - out - 1); }
            for (size_t i = 0; i < len; ++i) *out++ = tstart[i];
            p++;
        } else if (*p == 'u') {
            unsigned int uv = va_arg(ap, unsigned int);
            char tmp[12];
            char *tend = tmp + sizeof(tmp);
            char *tstart = uint_to_dec_str(uv, tend);
            size_t len = (size_t)(tend - tstart);
            if (out + len >= end) len = (size_t)(end - out - 1);
            for (size_t i = 0; i < len; ++i) *out++ = tstart[i];
            p++;
        } else if (*p == 's') {
            const char *s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            while (*s && out + 1 < end) { *out++ = *s++; }
            p++;
        } else if (*p == 'c') {
            int c = va_arg(ap, int);
            if (out + 1 < end) *out++ = (char)c;
            p++;
        } else {
            if (out + 1 < end) *out++ = *p;
            p++;
        }
    }

    *out = '\0';
    va_end(ap);
    return (int)(out - buf);
}

/* ---------- Формат размеров (компактный) ---------- */
static void format_bytes(uint64_t bytes, char *out, size_t out_size) {
    const char* units[] = {"B","KB","MB","GB","TB"};
    double size = (double)bytes;
    int unit_idx = 0;
    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        unit_idx++;
    }
    int whole = (int)size;
    /* Для компактного формата без точки используем целое значение */
    unsigned int val = (unsigned int)whole;
    /* Запись: например "512KB" */
    print_to_buf(out, out_size, "%u%s", val, units[unit_idx]);
}

/* ---------- Получаем строку CPU Brand через CPUID 0x80000002..4 ---------- */
static void get_cpu_brand_string(char *out) {
    if (!out) return;
    uint32_t *p = (uint32_t*)out;
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t a, b, c, d;
        cpuid(0x80000002 + i, 0, &a, &b, &c, &d);
        p[i*4 + 0] = a;
        p[i*4 + 1] = b;
        p[i*4 + 2] = c;
        p[i*4 + 3] = d;
    }
    out[48] = '\0';
}

static int my_atoi(const char *str) {
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

/* ---------- Главная функция вывода спецификаций ---------- */
void get_PC_specifications(char ***specs_out) {
    static char specs[12][64];
    static char *ptrs[13];
    static char ram_info[64];
    static char ram_used_info[64];
    static char cpu_brand[49];  /* 48 + \0 */

    uint64_t ram_total = get_ram_size_bytes();
    uint64_t ram_used  = get_ram_used_bytes();

    format_bytes(ram_total, ram_info, sizeof(ram_info));
    format_bytes(ram_used, ram_used_info, sizeof(ram_used_info));
    get_cpu_brand_string(cpu_brand);

    print_to_buf(specs[0], sizeof(specs[0]), "ParrotOS");
    print_to_buf(specs[1], sizeof(specs[1]), "OS: ParrotOS");
    print_to_buf(specs[2], sizeof(specs[2]), "Version OS: ParrotOS 0.0.0.0.2");
    print_to_buf(specs[3], sizeof(specs[3]), "Version kernel: ParrotOS POSK pra 2");
    print_to_buf(specs[4], sizeof(specs[4]), "Vendor: ParrotSoft");
    print_to_buf(specs[5], sizeof(specs[5]), "CPU: %s", cpu_brand);

    // Конвертируем в числа для вычисления процента
    int total = my_atoi(ram_info);
    int used  = my_atoi(ram_used_info);

    float percent = 0;
    if (total > 0) {
        percent = (used / (float)total) * 100.0f;
    }

    // Вывод с процентами
    print_to_buf(specs[6], sizeof(specs[6]),
                 "RAM total: %s Used: %s (%.2f%%)",
                 ram_info, ram_used_info, percent);

    specs[8][0] = '\0';

    for (int i = 0; i < 9; ++i) ptrs[i] = specs[i];
    ptrs[9] = NULL;
    *specs_out = ptrs;
}


#endif /* SYS_H */
