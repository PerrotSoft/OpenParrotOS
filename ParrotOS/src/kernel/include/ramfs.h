#ifndef RAMFS_H
#define RAMFS_H

#include <stdint.h>

#define RAMFS_MAX_FILES 32
#define RAMFS_MAX_NAME_LEN 32
#define RAMFS_STORAGE_SIZE (1024 * 1024) // 1 Мб

typedef struct {
    char name[RAMFS_MAX_NAME_LEN];
    uint8_t *data;
    uint32_t size;       // текущий размер файла
    uint32_t capacity;   // выделено памяти под файл
    uint8_t used;        // флаг занятости
} ramfs_file_t;

/* Инициализация RAMFS (очистка) */
void ramfs_init(void);

/* Создать файл с размером size (байт) */
int ramfs_create(const char *name, uint32_t size);

/* Записать данные в файл (offset - смещение в файле) */
int ramfs_write(const char *name, const void *data, uint32_t size, uint32_t offset);

/* Прочитать данные из файла (offset - смещение) */
int ramfs_read(const char *name, void *buffer, uint32_t size, uint32_t offset);

/* Удалить файл по имени */
int ramfs_delete(const char *name);

/* Получить указатель на файл по имени */
ramfs_file_t* ramfs_get_file(const char *name);

/* Вывести список файлов (вызывает callback для каждого) */
void ramfs_list(void (*callback)(const ramfs_file_t* file));

int ramfs_find_file_index(const char *name);
#endif // RAMFS_H
