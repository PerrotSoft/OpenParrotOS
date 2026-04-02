#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#define HEAP_SIZE 1024 * 1024  // 1 MB

static uint8_t heap[HEAP_SIZE];
static uint32_t heap_index = 0;

void* malloc(uint32_t size) {
    if (heap_index + size > HEAP_SIZE) return 0;
    void* ptr = &heap[heap_index];
    heap_index += size;
    return ptr;
}

void free(void* ptr) {
    // В примитивной версии free не реализован
}

#endif
