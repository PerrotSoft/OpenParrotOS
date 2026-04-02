#include "../tasks.h"
#include <stdlib.h>

static Task tasks[MAX_TASKS];
static int current_task = 0;
static int total_tasks = 0;

void switch_task(uint32_t *old_esp, uint32_t *old_ebp, uint32_t *old_eip,
                 uint32_t new_esp, uint32_t new_ebp, uint32_t new_eip)
{

}

void task_create(void (*func)()) {
    if (total_tasks >= MAX_TASKS) return;

    // Выделяем стек (4Кб)
    uint8_t* stack = (uint8_t*)malloc(4096);
    if (!stack) return;  // Проверка malloc

    uint32_t stack_top = (uint32_t)(stack + 4096);

    tasks[total_tasks].esp = stack_top;
    tasks[total_tasks].ebp = stack_top;
    tasks[total_tasks].eip = (uint32_t)func;
    total_tasks++;
}

void task_switch() {
    if (total_tasks < 2) return;

    int next_task = (current_task + 1) % total_tasks;

    switch_task(&tasks[current_task].esp, &tasks[current_task].ebp, &tasks[current_task].eip,
                tasks[next_task].esp, tasks[next_task].ebp, tasks[next_task].eip);

    current_task = next_task;
}
