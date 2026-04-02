#pragma once
#include <stdint.h>
#include "io.h"

static volatile uint32_t ticks = 0;

static void timer_callback() {
    ticks++;
}

void pit_init(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);
}

uint32_t timer_ticks() {
    return ticks;
}