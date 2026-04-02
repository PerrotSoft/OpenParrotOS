#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stdlib.h>
#include "memory.h"
#include "drivers/disck_driver.h"
#include "io.h"
#include "sys.h"
#define SIZE_NAME 100
#define FILE_HEADER_SIZE (1 + SIZE_NAME + 4)  // 1 байт статус + имя + размер

// Таблица файлов (для сохранения в начале диска)
struct fs_t_files {
    uint8_t name[SIZE_NAME];
    uint32_t adres;  // номер сектора, где начинается файл
};

// Описание файла после чтения
struct fs_files {
    uint8_t name[SIZE_NAME];
    uint32_t size;    // в секторах
    uint8_t* data;    // динамический массив данных
};

struct file_type{
    enum file_type_sys {
        file_unknown,
        file_sys,
        file_txt,
        file_bmp,
        file_wav,
        file_bin,
        file_info,
        file_dat
    } type;
    struct fs_files file;
};

uint16_t size_drive = 2048;   // количество секторов на диске
uint16_t table_size = 0;      // сколько секторов выделено под таблицу
struct fs_t_files* t_fs = NULL;
uint16_t real_file_count = 0; // сколько записей влезает в таблицу
uint16_t current_file_count = 0; // сколько реально файлов создано

void clear_drive(){
    uint8_t ler[512];
    for (int i = 0; i < 1000; i++)
    {
        ide_write_sector(i,(uint16_t*)ler);
    }
    
}
// Инициализация таблицы файлов из диска
void init_fs(uint16_t table_size1) {
    table_size = table_size1;

    uint8_t* disk_buffer = (uint8_t*) malloc(table_size * 512);
    if (!disk_buffer) return;

    for (uint16_t i = 0; i < table_size; i++) {
        ide_read_sector(i, (uint16_t*)(disk_buffer + i * 512));
    }

    real_file_count = (table_size * 512) / sizeof(struct fs_t_files);
    t_fs = (struct fs_t_files*) malloc(real_file_count * sizeof(struct fs_t_files));
    if (!t_fs) {
        free(disk_buffer);
        return;
    }

    memcpy(t_fs, disk_buffer, real_file_count * sizeof(struct fs_t_files));

    current_file_count = 0;
    for (uint16_t i = 0; i < real_file_count; i++) {
        if (t_fs[i].name[0] != 0) {
            current_file_count++;
        }
    }

    free(disk_buffer);
}


// Сериализация таблицы файлов в буфер
uint8_t* fs_to_buffer(uint16_t table_size) {
    if (!t_fs || current_file_count == 0) return NULL;

    uint32_t total_size = table_size * 512;
    uint8_t* buffer = (uint8_t*) malloc(total_size);
    if (!buffer) return NULL;

    memset(buffer, 0, total_size);
    memcpy(buffer, t_fs, current_file_count * sizeof(struct fs_t_files));

    return buffer;
}


// Сохранение таблицы файлов на диск
int save_fs(uint16_t table_size) {
    if (!t_fs || current_file_count == 0) return -1;

    uint8_t* buf = fs_to_buffer(table_size);
    if (!buf) return -2;

    for (uint16_t i = 0; i < table_size; i++) {
        ide_write_sector(i, (uint16_t*)(buf + i * 512));
    }

    free(buf);
    return 0;
}


// Добавить запись в таблицу
int create_to_t_fs(const char* name, uint32_t address) {
    if (!t_fs) return -1;
    if (current_file_count >= real_file_count) return -2;

    strncpy((char*)t_fs[current_file_count].name, name, SIZE_NAME - 1);
    t_fs[current_file_count].name[SIZE_NAME - 1] = '\0';
    t_fs[current_file_count].adres = address;

    current_file_count++;
    return save_fs(table_size);
}

bool exen_file(const char* name){
    for (size_t i = 0; i < real_file_count; i++)
    if(t_fs[i].name == name) return true;
    return false;
}

// Создать файл и записать данные
void create_file(const char* name, uint32_t size_sectors, const uint8_t* data) {
    if (!t_fs) return;
    if (current_file_count >= real_file_count) return;
    if (size_sectors == 0) size_sectors = 1;

    uint32_t address = table_size + current_file_count * size_sectors; // простейшее распределение

    uint8_t sector_buf[512];
    memset(sector_buf, 0, 512);

    // Заголовок файла
    sector_buf[0] = 1; // файл существует
    strncpy((char*)(sector_buf + 1), name, SIZE_NAME);
    *((uint32_t*)(sector_buf + 1 + SIZE_NAME)) = size_sectors;

    // Копируем часть данных в первый сектор (после заголовка)
    if (data) {
        memcpy(sector_buf + FILE_HEADER_SIZE, data, 512 - FILE_HEADER_SIZE);
    }
    ide_write_sector(address, (uint16_t*)sector_buf);

    // Записываем таблицу
    create_to_t_fs(name, address);

    // Оставшиеся сектора
    for (uint32_t i = 1; i < size_sectors; i++) {
        memset(sector_buf, 0, 512);
        if (data) {
            uint32_t offset = i * 512 - FILE_HEADER_SIZE;
            uint32_t max_bytes = size_sectors * 512 - FILE_HEADER_SIZE;
            uint32_t bytes_to_copy = (offset + 512 <= max_bytes)
                                   ? 512 : (max_bytes - offset);
            memcpy(sector_buf, data + offset, bytes_to_copy);
        }
        ide_write_sector(address + i, (uint16_t*)sector_buf);
    }
}


// Прочитать файл
struct fs_files read_file(const char* name) {
    struct fs_files result;
    memset(&result, 0, sizeof(result));

    if (!t_fs) return result;

    for (uint16_t i = 0; i < current_file_count; i++) {
        uint32_t address = t_fs[i].adres;
        uint8_t sector[512];
        ide_read_sector(address, (uint16_t*)sector);

        if (sector[0] == 0) continue; // пустой файл

        char file_name[SIZE_NAME + 1];
        memcpy(file_name, sector + 1, SIZE_NAME);
        file_name[SIZE_NAME] = '\0';

        if (strcmp(file_name, name) == 0) {
            result.size = *((uint32_t*)(sector + 1 + SIZE_NAME));
            if (result.size == 0) result.size = 1;

            strncpy((char*)result.name, name, SIZE_NAME - 1);
            result.name[SIZE_NAME - 1] = '\0';

            result.data = malloc(result.size * 512);
            if (!result.data) break;

            // первый сектор
            memcpy(result.data, sector + FILE_HEADER_SIZE, 512 - FILE_HEADER_SIZE);

            // остальные
            for (uint32_t s = 1; s < result.size; s++) {
                ide_read_sector(address + s, (uint16_t*)sector);
                memcpy(result.data + s * 512 - FILE_HEADER_SIZE, sector, 512);
            }
            break;
        }
    }
    return result;
}
// Изменить содержимое файла (перезапись)
int write_file(const char* name, const uint8_t* data, uint32_t size_bytes) {
    if (!t_fs) return -1;

    for (uint16_t i = 0; i < current_file_count; i++) {
        uint32_t address = t_fs[i].adres;
        uint8_t sector[512];
        ide_read_sector(address, (uint16_t*)sector);

        if (sector[0] == 0) continue; // пустой файл

        char file_name[SIZE_NAME + 1];
        memcpy(file_name, sector + 1, SIZE_NAME);
        file_name[SIZE_NAME] = '\0';

        if (strcmp(file_name, name) == 0) {
            // вычисляем размер в секторах
            uint32_t old_size_sectors = *((uint32_t*)(sector + 1 + SIZE_NAME));
            if (old_size_sectors == 0) old_size_sectors = 1;

            uint32_t new_size_sectors = (size_bytes + FILE_HEADER_SIZE + 511) / 512;
            if (new_size_sectors > old_size_sectors) {
                // файл больше, чем выделено секторов → ошибка (или можно перераспределять)
                return -2;
            }

            // первый сектор (с заголовком)
            memset(sector, 0, 512);
            sector[0] = 1; // существует
            strncpy((char*)(sector + 1), name, SIZE_NAME);
            *((uint32_t*)(sector + 1 + SIZE_NAME)) = old_size_sectors;

            if (data) {
                uint32_t to_copy = (size_bytes < (512 - FILE_HEADER_SIZE)) 
                                    ? size_bytes : (512 - FILE_HEADER_SIZE);
                memcpy(sector + FILE_HEADER_SIZE, data, to_copy);
            }
            ide_write_sector(address, (uint16_t*)sector);

            // остальные сектора
            for (uint32_t s = 1; s < old_size_sectors; s++) {
                memset(sector, 0, 512);
                if (data) {
                    int offset = s * 512 - FILE_HEADER_SIZE;
                    if (offset < (int)size_bytes) {
                        uint32_t remain = size_bytes - offset;
                        uint32_t copy_len = (remain >= 512) ? 512 : remain;
                        memcpy(sector, data + offset, copy_len);
                    }
                }
                ide_write_sector(address + s, (uint16_t*)sector);
            }
            return 0; // успех
        }
    }
    return -3; // файл не найден
}
int delete_file(const char* name) {
    if (!t_fs) return -1;

    for (uint16_t i = 0; i < current_file_count; i++) {
        uint32_t address = t_fs[i].adres;
        uint8_t sector[512];
        ide_read_sector(address, (uint16_t*)sector);

        if (sector[0] == 0) continue;

        char file_name[SIZE_NAME + 1];
        memcpy(file_name, sector + 1, SIZE_NAME);
        file_name[SIZE_NAME] = '\0';

        if (strcmp(file_name, name) == 0) {
            uint32_t size_sectors = *((uint32_t*)(sector + 1 + SIZE_NAME));
            if (size_sectors == 0) size_sectors = 1;

            // Обнуляем все сектора файла
            memset(sector, 0, 512);
            for (uint32_t s = 0; s < size_sectors; s++) {
                ide_write_sector(address + s, (uint16_t*)sector);
            }

            // Удаляем из таблицы
            for (uint16_t j = i; j < current_file_count - 1; j++) {
                t_fs[j] = t_fs[j + 1];
            }
            current_file_count--;
            return 0; // успех
        }
    }
    return -2; // файл не найден
}

int rename_file(const char* old_name, const char* new_name) {
    if (!t_fs) return -1;

    for (uint16_t i = 0; i < current_file_count; i++) {
        uint32_t address = t_fs[i].adres;
        uint8_t sector[512];
        ide_read_sector(address, (uint16_t*)sector);

        if (sector[0] == 0) continue;

        char file_name[SIZE_NAME + 1];
        memcpy(file_name, sector + 1, SIZE_NAME);
        file_name[SIZE_NAME] = '\0';

        if (strcmp(file_name, old_name) == 0) {
            memset(sector + 1, 0, SIZE_NAME);
            strncpy((char*)(sector + 1), new_name, SIZE_NAME - 1);
            ide_write_sector(address, (uint16_t*)sector);

            // Также обновляем таблицу
            strncpy(t_fs[i].name, new_name, SIZE_NAME - 1);
            t_fs[i].name[SIZE_NAME - 1] = '\0';
            return 0; // успех
        }
    }
    return -2; // файл не найден
}
bool copy_file(const char* src_name, const char* dst_name) {
    //if(!exen_file(src_name)) return false;
    struct fs_files f = read_file(src_name);
    //if(exen_file(src_name)) return false;
     create_file(dst_name, f.size, f.data);
    free(f.data);
    return true;
}

struct file_type open_to_type(const char* name) {
    struct file_type type;
    type.type = file_unknown;
    type.file = read_file(name); // сразу читаем файл

    if (!type.file.data) return type; // файл не найден

    char** parts = str(name, '.'); // разделение по точкам
    int count = 0;
    while (parts[count] != 0) count++;

    if (count > 1) {
        const char* ext = parts[count - 1]; // берем последнюю часть после точки
        if (strcmp(ext, "txt") == 0) type.type = file_txt;
        else if (strcmp(ext, "sys") == 0) type.type = file_sys;
        else if (strcmp(ext, "bmp") == 0) type.type = file_bmp;
        else if (strcmp(ext, "wav") == 0) type.type = file_wav;
        else if (strcmp(ext, "bin") == 0) type.type = file_bin;
        else if (strcmp(ext, "info") == 0) type.type = file_info;
        else if (strcmp(ext, "dat") == 0) type.type = file_dat;
        else type.type = file_unknown;
    }

    return type;
}

uint8_t* open_file(const char* name) {
    struct file_type type = open_to_type(name);
    if (!type.file.data) return NULL; // файл не найден

    if (type.type == file_sys) {
        // запуск бинарника
        if (execute_binary_rint(type.file.data) == 0) {
            // успех
            return 0; 
        } else {
            static uint8_t data_example[] = {0x01};
            return data_example;

        }
    } else if (type.type == file_txt || type.type == file_bin || type.type == file_dat || type.type == file_info) {
        return type.file.data; // возвращаем содержимое
    } else {
        return type.file.data; // для остальных файлов возвращаем как есть (например, bmp, wav)
    }
}

#endif