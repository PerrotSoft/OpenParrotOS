#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

#define IDT_SIZE 256

static idt_entry_t idt[IDT_SIZE];
static idt_ptr_t idt_ptr;

// Установка одной записи IDT
static void idt_set_gate(uint8_t n, uint32_t handler_addr, uint16_t sel, uint8_t flags) {
    idt[n].offset_low  = handler_addr & 0xFFFF;
    idt[n].selector    = sel;
    idt[n].zero        = 0;
    idt[n].type_attr   = flags;  // 0x8E = P=1, DPL=0, type=interrupt gate
    idt[n].offset_high = (handler_addr >> 16) & 0xFFFF;
}

// Добавление обработчика
void add_interrupt_handler(uint8_t int_num, void (*handler)(void)) {
    if (int_num >= IDT_SIZE) return; // защита от выхода за массив
    idt_set_gate(int_num, (uint32_t)handler, 0x08, 0x8E);
}

// Загрузка IDT в процессор
void lidt(void* idt_ptr) {
    asm volatile (
        "lidt (%0)"
        :
        : "r"(idt_ptr)
        : "memory"
    );
}


void idt_install(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint32_t)&idt;

    // Обнуление всех векторов
    for (int i = 0; i < IDT_SIZE; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    lidt(&idt_ptr); // загрузка IDT в регистр IDTR
}
