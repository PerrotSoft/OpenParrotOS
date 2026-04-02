#include "../io.h"
#include <stdbool.h>
#include <stdlib.h>
uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__ ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uintptr_t virt_to_phys(void* addr) {
    return (uintptr_t)addr; // если нет MMU
}

int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}
char* strcpy(char* dst, const char* src) {
    char* r = dst;
    while ((*dst++ = *src++));
    return r;
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
int strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        unsigned char c1 = (unsigned char)s1[i];
        unsigned char c2 = (unsigned char)s2[i];
        if (c1 != c2) return c1 - c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

char* strncpy(char* dest, const char* src, int n) {
    int i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

void uint8_to_hex(uint8_t byte, char out[3]) {
    const char* hex_chars = "0123456789ABCDEF";
    out[0] = hex_chars[(byte >> 4) & 0x0F];
    out[1] = hex_chars[byte & 0x0F];
    out[2] = '\0';
}

void* memcpy(void* dest, const void* src, int n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (int i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void* memmove(void* dest, const void* src, int n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    if (d < s) {
        for (int i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (int i = n; i != 0; i--) d[i-1] = s[i-1];
    }
    return dest;
}

void* memset(void* dest, int val, unsigned int len) {
    unsigned char* ptr = dest;
    while (len-- > 0) *ptr++ = (unsigned char)val;
    return dest;
}
