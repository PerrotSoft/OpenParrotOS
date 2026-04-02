#ifndef IO_H
#define IO_H

#include <stdint.h>

uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, uint16_t val);

char* st(const char* text, int sti);
char** str(const char* text, char s);
char* strcpy(char* dst, const char* src);
int strncmp(const char* s1, const char* s2, int n);
char* strncpy(char* dest, const char* src, int n);
void uint8_to_hex(uint8_t byte, char out[3]);
void* memcpy(void* dest, const void* src, int n);
int strcmp(const char* s1, const char* s2);
void* memmove(void* dest, const void* src, int n);
void* memset(void* dest, int val, unsigned int len);
static inline uint16_t inw(uint16_t port);
int strlen(const char* str);
static inline void outl(uint16_t port, uint32_t val);
static inline uintptr_t virt_to_phys(void* addr);
#endif // IO_H
