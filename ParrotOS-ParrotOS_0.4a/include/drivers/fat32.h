#ifndef FAT32_H
#define FAT32_H

#include <efi.h>
#include <efilib.h>

#define MAX_PATH_LEN 512
#define MAX_DISKS 52

struct EC16
{
    EFI_STATUS Status;
    CHAR16 *Message;
    UINTN FileSize;
};
// Тип диска
typedef enum {
    DISK_A = 0,   // встроенный (например, ROM)
    DISK_B,       // EEPROM
    DISK_C,       // HDD / виртуальный образ (boot.img)
    DISK_D,       // второй подключенный диск (extra.img)
    DISK_MAX
} DISK_TYPE;

// Структура описания диска
typedef struct {
    CHAR16 Letter;        // буква диска (A, B, C, D...)
    EFI_FILE_HANDLE Root; // корневой каталог
    BOOLEAN Mounted;      // подключен ли
} Disk;
extern CHAR16 FAT32_CurrentDisk;


// ===== Текущий путь и диск =====
extern EFI_FILE_PROTOCOL *FAT32_CWD;
extern CHAR16 FAT32_CurrentPath[MAX_PATH_LEN];
extern EFI_FILE_PROTOCOL *FAT32_Disks[MAX_DISKS];
extern CHAR16 FAT32_DiskLetters[MAX_DISKS];
extern UINTN FAT32_DiskCount;
// Глобальный список дисков
extern Disk Disks[DISK_MAX];

// Инициализация системы дисков
EFI_STATUS DiskSystem_Init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS FAT32_ReadFileByPath(CHAR16 *path_in, struct EC16 *out);
// Получение диска по букве
Disk* Disk_Get(CHAR16 Letter);

// Смена текущего диска (например, "C:")
EFI_STATUS Disk_SetCurrent(CHAR16 Letter);

// Текущий путь (например, "C:\folder\file.txt")
const CHAR16* Disk_GetCurrentPath();

// Навигация по ".." (подняться на уровень выше)
EFI_STATUS Disk_PathUp();

// Присоединение дополнительного диска (например, extra.img как D:)
EFI_STATUS Disk_MountExtra(EFI_HANDLE ImageHandle, CHAR16 Letter);
// ===== Вспомогательные функции =====
UINTN FAT32_SplitLine(CHAR16 *line, CHAR16 *args[], UINTN max_args);
EFI_FILE_PROTOCOL* FAT32_GetRoot(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST);

// ===== Работа с дисками =====
void FAT32_RegisterDisk(CHAR16 letter, EFI_FILE_PROTOCOL *root);
EFI_STATUS FAT32_ChangeDisk(CHAR16 letter);
CHAR16* FAT32_PrintCurrentPath();
struct EC16 FAT32_ListDisks();
// ===== Директории =====
EFI_STATUS FAT32_CreateDir(CHAR16 *name);
EFI_STATUS FAT32_DeleteDir(CHAR16 *name);
struct EC16 FAT32_ListDir();
EFI_STATUS FAT32_ChangeDirEx(CHAR16 *path);
void Fat32_RegisterrsDisk();
// ===== Файлы =====
EFI_STATUS FAT32_CreateFile(CHAR16 *name);
EFI_STATUS FAT32_DeleteFile(CHAR16 *name);
struct EC16 FAT32_ReadFile(CHAR16 *filename);
EFI_STATUS FAT32_WriteFile(CHAR16 *filename, UINT16 *data, UINTN len);
EFI_STATUS FAT32_AppendFile(CHAR16 *filename, UINT16 *data, UINTN len);

// ===== Копирование и перемещение =====
EFI_STATUS FAT32_CopyFile(CHAR16 *src, CHAR16 *dest);
EFI_STATUS FAT32_MoveFile(CHAR16 *src, CHAR16 *dest);
EFI_STATUS FAT32_DeleteEntry(CHAR16 *name);
struct EC16* FAT32_ListSimple(UINTN *SizeOut);

// ===== Получение размера файла =====
EFI_STATUS FAT32_GetFileSize(CHAR16 *filename, UINT64 *filesize);

#endif // FAT32_H
