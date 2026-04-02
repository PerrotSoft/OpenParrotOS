#include <stdint.h>
#include <stdbool.h>
#include "include/cmd_POS.h"
#include "include/interrupts.h"
#include "include/tasks.h"
#include "include/memory.h"
#include "include/timer.h"
#include "include/console.h"

void boot_meneger() {
    VGA_COLOR = 0xA0;
    fill_screen(-1);

    uint8_t num = 2;

    while (true) {
        // Обновляем отображение меню
        set_cursor(9, 39);
        print_char(24); // ↑
        print_char(25); // ↓

        if (num == 1) {
            VGA_COLOR = 0x2F;
            set_cursor(10, 35);
            print("fill boot");
            VGA_COLOR = 0xA0;
            set_cursor(11, 36);
            print("cmd boot");
        } else {
            VGA_COLOR = 0xA0;
            set_cursor(10, 35);
            print("fill boot");
            VGA_COLOR = 0x2F;
            set_cursor(11, 36);
            print("cmd boot");
            VGA_COLOR = 0xA0;
        }

        char input = read_char();

        switch (input) {
            case SC_UP:
                num = 1;
                break;
            case SC_Down:
                num = 2;
                break;
            case '\n':
            case '\r': // иногда Enter может быть '\r'
                switch (num) {
                    case 1:
                        print("Selected fill boot...\n");
                        break;
                    case 2:
                        VGA_COLOR = 0x0F;
                        fill_screen(-1);
                        main_cmd();  // запускаем командную оболочку
                        return;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
}
void load_interrupt();
void kernel_main(uint32_t memory_map_address) {
    print("ParrotOS KERNEL\n");
    load_interrupt();
    pit_init(50); // таймер 50 Гц

    while(true)
    boot_meneger();
    while (1) {
        while (1) {
        if (timer_ticks() % 50 == 0) { // переключаем каждые 50 тиков
            task_switch();
        }
    }
    }
}
// Обёртка для передачи параметра в fatal_error
#define DEFINE_FATAL_WRAPPER(num, msg) \
    void isr_##num() { fatal_error(msg); }

// Создаём обёртки для нужных исключений
DEFINE_FATAL_WRAPPER(0x00, "Exception: Divide Error (0x00)")
DEFINE_FATAL_WRAPPER(0x06, "Exception: Invalid Opcode (0x06)")
DEFINE_FATAL_WRAPPER(0x08, "Exception: Double Fault (0x08)")
DEFINE_FATAL_WRAPPER(0x0D, "Exception: General Protection Fault (0x0D)")
DEFINE_FATAL_WRAPPER(0x0E, "Exception: Page Fault (0x0E)")
void int_input_and_output_sys();
void load_interrupt() {
    // CPU исключения
    add_interrupt_handler(0x00, isr_0x00);
    add_interrupt_handler(0x06, isr_0x06);
    add_interrupt_handler(0x08, isr_0x08);
    add_interrupt_handler(0x0D, isr_0x0D);
    add_interrupt_handler(0x0E, isr_0x0E);

    // Системное ввод/вывод
    add_interrupt_handler(0x21, int_input_and_output_sys);
    add_interrupt_handler(0x22, isr_0x00);
    add_interrupt_handler(32, timer_callback);
    idt_install();
}

void int_input_and_output_sys(void) {
    uint32_t eax;
    uint32_t esi;

    /* Считаем полные регистры — так проще и безопаснее с inline-asm. */
    asm volatile("mov %%eax, %0" : "=r"(eax));
    asm volatile("mov %%esi, %0" : "=r"(esi));

    uint8_t ah = (eax >> 8) & 0xFF;   // AH = старший байт регистра AX
    uint8_t al = eax & 0xFF;         // AL = младший байт

    switch (ah) {
        case 0: { // вывод символа из AL
            print_char((char)al);
            break;
        }

        case 1: { // печать строки по адресу в ESI
            const char *ptr = (const char *)esi;
            print(ptr);
            break;
        }

        case 2: { // заполнение экрана значением в AL
            fill_screen(al);
            break;
        }

        case 3: { // чтение символа и возвращение его в AL
            char r = read_char();                 // предполагаем, что read_char() возвращает char
            uint32_t new_eax = (uint32_t)((uint8_t)r); // поместим символ в AL (младший байт EAX)
            asm volatile("mov %0, %%eax" : : "r"(new_eax) : "eax");
            break;
        }

        case 4: { // чтение строки в буфер и возвращение указателя на неё в ESI
            static char buf[32];     // static чтобы указатель оставался валиден после выхода
            read_line(buf, sizeof(buf));
            asm volatile("mov %0, %%esi" : : "r"(buf) : "esi");
            break;
        }

        default:
            break;
    }

    /* Примечание: если у тебя ISR должен вернуть управление с сохранёнными регистрами,
       убедись, что стек/регистры сохраняются/восстанавливаются корректно в обёртке прерывания. */
}
