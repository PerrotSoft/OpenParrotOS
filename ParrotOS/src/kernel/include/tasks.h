#ifndef TASKS_H
#define TASKS_H

#include <stdint.h>

#define MAX_TASKS 2

typedef struct {
    uint32_t esp, ebp, eip;
} Task;

void task_create(void (*func)());
void task_switch();

#endif
