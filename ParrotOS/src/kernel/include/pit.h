#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "interrupts.h"

#define PIT_FREQUENCY 1193182

static volatile uint32_t tick_count = 0;

void pit_handler() {
    tick_count++;
}

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;

    // Устанавливаем обработчик IRQ0 (PIT)
    add_interrupt_handler(0x20, pit_handler);

    // Настройка PIT (канал 0, режим 3, двоичный счёт)
    asm volatile("outb %0, $0x43" : : "a"(0x36));
    asm volatile("outb %0, $0x40" : : "a"(divisor & 0xFF));
    asm volatile("outb %0, $0x40" : : "a"((divisor >> 8) & 0xFF));
}

uint32_t timer_ticks() {
    return tick_count;
}

#endif
