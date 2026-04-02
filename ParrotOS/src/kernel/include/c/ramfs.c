#include "../ramfs.h"
#include <string.h>

static uint8_t RAMFS_STORAGE[RAMFS_STORAGE_SIZE];
static uint32_t ramfs_storage_used = 0;

static ramfs_file_t ramfs_files[RAMFS_MAX_FILES];

void ramfs_init(void) {
    memset(RAMFS_STORAGE, 0, sizeof(RAMFS_STORAGE));
    memset(ramfs_files, 0, sizeof(ramfs_files));
    ramfs_storage_used = 0;
}

static int find_file_index(const char *name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (ramfs_files[i].used && strcmp(ramfs_files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}
int ramfs_find_file_index(const char *name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (ramfs_files[i].used && strcmp(ramfs_files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}
ramfs_file_t* ramfs_get_file(const char *name) {
    int idx = find_file_index(name);
    if (idx < 0) return NULL;
    return &ramfs_files[idx];
}

int ramfs_create(const char *name, uint32_t size) {
    if (size == 0 || size > (RAMFS_STORAGE_SIZE - ramfs_storage_used)) return -1;

    if (find_file_index(name) >= 0) return -2; // файл уже существует

    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!ramfs_files[i].used) {
            strncpy(ramfs_files[i].name, name, RAMFS_MAX_NAME_LEN - 1);
            ramfs_files[i].name[RAMFS_MAX_NAME_LEN - 1] = '\0';

            ramfs_files[i].data = &RAMFS_STORAGE[ramfs_storage_used];
            ramfs_files[i].capacity = size;
            ramfs_files[i].size = 0;
            ramfs_files[i].used = 1;

            ramfs_storage_used += size;

            // Обнулим выделенную область
            memset(ramfs_files[i].data, 0, size);

            return 0; // успех
        }
    }

    return -3; // нет свободных дескрипторов
}

int ramfs_write(const char *name, const void *data, uint32_t size, uint32_t offset) {
    int idx = find_file_index(name);
    if (idx < 0) return -1;

    ramfs_file_t *file = &ramfs_files[idx];
    if (offset + size > file->capacity) return -2; // превышение выделенной памяти

    memcpy(file->data + offset, data, size);

    // Обновляем размер файла, если записали дальше текущего
    if (offset + size > file->size) {
        file->size = offset + size;
    }
    return 0;
}

int ramfs_read(const char *name, void *buffer, uint32_t size, uint32_t offset) {
    int idx = find_file_index(name);
    if (idx < 0) return -1;

    ramfs_file_t *file = &ramfs_files[idx];
    if (offset >= file->size) return 0; // чтение за пределами файла — ничего читать

    if (offset + size > file->size) {
        size = file->size - offset; // читаем только доступный размер
    }

    memcpy(buffer, file->data + offset, size);
    return (int)size;
}

int ramfs_delete(const char *name) {
    int idx = find_file_index(name);
    if (idx < 0) return -1;

    ramfs_file_t *file = &ramfs_files[idx];

    // Очистим область памяти (необязательно, но чисто)
    memset(file->data, 0, file->capacity);

    file->used = 0;

    // Не делаем уменьшение ramfs_storage_used, чтобы упростить реализацию
    // (сложная задача — реаллокация)

    return 0;
}

void ramfs_list(void (*callback)(const ramfs_file_t* file)) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (ramfs_files[i].used && callback) {
            callback(&ramfs_files[i]);
        }
    }
}
