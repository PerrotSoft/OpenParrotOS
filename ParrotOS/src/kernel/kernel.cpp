#include <stdint.h>
#include "console.h"
// Точка входа ядра
extern "C" void kernel_main(uint32_t memory_map_address) {
    print("ParrotOS KERNEL\n");
    print("Enter your name: ");

    char name[64];
    read_line(name, 64);

    print("\nHello, ");
    print(name);
    print("!\n");

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
