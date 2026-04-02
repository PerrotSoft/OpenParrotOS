#ifndef SYS_H
#define SYS_H
#include <stdint.h>

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

void execute_binary(uint8_t* code) {
    asm volatile(
        // --- сохраняем регистры вручную вместо pushad ---
        "push %%eax\n\t"
        "push %%ebx\n\t"
        "push %%ecx\n\t"
        "push %%edx\n\t"
        "push %%esi\n\t"
        "push %%edi\n\t"
        "push %%ebp\n\t"

        // --- вызываем бинарный код через указатель ---
        "call *%0\n\t"

        // --- восстанавливаем регистры вручную вместо popad ---
        "pop %%ebp\n\t"
        "pop %%edi\n\t"
        "pop %%esi\n\t"
        "pop %%edx\n\t"
        "pop %%ecx\n\t"
        "pop %%ebx\n\t"
        "pop %%eax\n\t"
        :
        : "r"(code)
        : "memory"
    );
}
int execute_binary_rint(uint8_t* code) {
    asm volatile(
        "push %%ebx\n\t"
        "push %%ecx\n\t"
        "push %%edx\n\t"
        "push %%esi\n\t"
        "push %%edi\n\t"
        "push %%ebp\n\t"

        // --- вызываем бинарный код через указатель ---
        "call *%0\n\t"

        // --- восстанавливаем регистры вручную вместо popad ---
        "pop %%ebp\n\t"
        "pop %%edi\n\t"
        "pop %%esi\n\t"
        "pop %%edx\n\t"
        "pop %%ecx\n\t"
        "pop %%ebx\n\t"
        :
        : "r"(code)
        : "memory"
    );
}
char execute_binary_rchar(uint8_t* code) {
    asm volatile(
        // --- сохраняем регистры вручную вместо pushad ---
        "push %%ebx\n\t"
        "push %%ecx\n\t"
        "push %%edx\n\t"
        "push %%esi\n\t"
        "push %%edi\n\t"
        "push %%ebp\n\t"

        // --- вызываем бинарный код через указатель ---
        "call *%0\n\t"

        // --- восстанавливаем регистры вручную вместо popad ---
        "pop %%ebp\n\t"
        "pop %%edi\n\t"
        "pop %%esi\n\t"
        "pop %%edx\n\t"
        "pop %%ecx\n\t"
        "pop %%ebx\n\t"
        :
        : "r"(code)
        : "memory"
    );
}

#endif /* SYS_H */

