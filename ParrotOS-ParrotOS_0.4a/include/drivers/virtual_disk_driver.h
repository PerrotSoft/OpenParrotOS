#ifndef VIRTUAL_DISK_DRIVER_H
#define VIRTUAL_DISK_DRIVER_H

#include <efi.h>
#include <efilib.h>
#include "FAT32.h" // твой заголовок с FAT32_GetRoot

#define SECTOR_SIZE 512
#define MAX_DISKS 4
#define DISK_DAT_NAME L"disk.dat"
#define DEFAULT_DISK_NAME L"fscs.bin"
#define DEFAULT_SECTORS 2048ULL // 2048 * 512 = 1 MiB

// коды ошибок (упрощённо)
#define VDISK_ERR_OK      0
#define VDISK_ERR_NODEV  -4
#define VDISK_ERR_IO     -5
#define VDISK_ERR_NOMEM  -6
#define VDISK_ERR_PARAM  -7

typedef struct {
    BOOLEAN used;
    CHAR16 name[128];
    EFI_FILE_PROTOCOL *fh;
    UINT64 size_bytes;
    UINT32 sectors;
} VDISK;

static VDISK vdisks[MAX_DISKS];
static UINT8 current_drive = 0;

static EFI_HANDLE gImageHandle = NULL;
static EFI_FILE_PROTOCOL *gRoot = NULL;

// Прототипы, чтобы не было implicit decl
EFI_STATUS ide_fat32_init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST);
void ide_select_drive(UINT8 drive);
int ide_read_sector(uint32_t lba, UINT8 *buffer);               // buffer - 512 bytes
int ide_write_sector(uint32_t lba, const UINT8 *buffer);       // buffer - 512 bytes
int ide_identify(uint16_t *buffer256);                         // 256 words
uint32_t ide_get_sector_count(void);
uint8_t ide_read_byte(uint32_t index);
void ide_write_byte(uint32_t index, uint8_t value);
void init_virtual_disk(UINT64 bytes); // удобный инициализатор в рантайме

// ===== Вспомогательные функции =====

static EFI_STATUS ensure_root(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) {
    if (gRoot) return EFI_SUCCESS;
    gRoot = FAT32_GetRoot(ImageHandle, ST);
    if (!gRoot) return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

// Чтение всего ASCII файла в буфер (AllocatePool). out_size - размер в байтах
static EFI_STATUS read_entire_file_ascii(CHAR16 *name, CHAR8 **out_buf, UINTN *out_size) {
    EFI_STATUS st;
    EFI_FILE_PROTOCOL *file;
    EFI_GUID fileInfoGuid = EFI_FILE_INFO_ID;
    UINTN info_size = 0;
    EFI_FILE_INFO *info;

    st = uefi_call_wrapper(gRoot->Open, 5, gRoot, &file, name, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(file->GetInfo, 4, file, &fileInfoGuid, &info_size, NULL);
    if (st != EFI_BUFFER_TOO_SMALL) {
        uefi_call_wrapper(file->Close, 1, file);
        return st;
    }
    info = AllocatePool(info_size);
    if (!info) { uefi_call_wrapper(file->Close, 1, file); return EFI_OUT_OF_RESOURCES; }
    st = uefi_call_wrapper(file->GetInfo, 4, file, &fileInfoGuid, &info_size, info);
    if (EFI_ERROR(st)) { FreePool(info); uefi_call_wrapper(file->Close, 1, file); return st; }

    UINTN size = (UINTN)info->FileSize;
    FreePool(info);

    CHAR8 *buf = AllocatePool(size + 1);
    if (!buf) { uefi_call_wrapper(file->Close, 1, file); return EFI_OUT_OF_RESOURCES; }

    UINTN read = size;
    uefi_call_wrapper(file->SetPosition, 2, file, 0);
    st = uefi_call_wrapper(file->Read, 3, file, &read, buf);
    if (EFI_ERROR(st)) { FreePool(buf); uefi_call_wrapper(file->Close, 1, file); return st; }
    buf[read] = 0;
    *out_buf = buf;
    *out_size = read;
    uefi_call_wrapper(file->Close, 1, file);
    return EFI_SUCCESS;
}

// Убедиться что файл существует и >= size_bytes (расширение нулями)
static EFI_STATUS ensure_backing_file_size(CHAR16 *name, UINT64 size_bytes) {
    EFI_STATUS st;
    EFI_FILE_PROTOCOL *file;
    EFI_GUID fileInfoGuid = EFI_FILE_INFO_ID;
    UINTN info_size = 0;
    EFI_FILE_INFO *info;

    st = uefi_call_wrapper(gRoot->Open, 5, gRoot, &file, name, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (EFI_ERROR(st)) {
        // создадим
        st = uefi_call_wrapper(gRoot->Open, 5, gRoot, &file, name,
                              EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
        if (EFI_ERROR(st)) return st;
    }

    st = uefi_call_wrapper(file->GetInfo, 4, file, &fileInfoGuid, &info_size, NULL);
    if (st != EFI_BUFFER_TOO_SMALL) { uefi_call_wrapper(file->Close, 1, file); return st; }
    info = AllocatePool(info_size);
    if (!info) { uefi_call_wrapper(file->Close, 1, file); return EFI_OUT_OF_RESOURCES; }
    st = uefi_call_wrapper(file->GetInfo, 4, file, &fileInfoGuid, &info_size, info);
    if (EFI_ERROR(st)) { FreePool(info); uefi_call_wrapper(file->Close, 1, file); return st; }

    UINT64 curr = info->FileSize;
    FreePool(info);

    if (curr < size_bytes) {
        UINT64 remain = size_bytes - curr;
        #define CHUNK 4096
        UINT8 *z = AllocatePool(CHUNK);
        if (!z) { uefi_call_wrapper(file->Close, 1, file); return EFI_OUT_OF_RESOURCES; }
        SetMem(z, CHUNK, 0);
        uefi_call_wrapper(file->SetPosition, 2, file, curr);
        while (remain) {
            UINTN w = (remain > CHUNK) ? CHUNK : (UINTN)remain;
            UINTN ws = w;
            st = uefi_call_wrapper(file->Write, 3, file, &ws, z);
            if (EFI_ERROR(st) || ws != w) { FreePool(z); uefi_call_wrapper(file->Close, 1, file); return EFI_DEVICE_ERROR; }
            remain -= w;
        }
        FreePool(z);
    }

    uefi_call_wrapper(file->Close, 1, file);
    return EFI_SUCCESS;
}

// Открыть дескриптор backing-файла
static EFI_STATUS open_backing_handle(UINTN idx) {
    if (idx >= MAX_DISKS) return EFI_INVALID_PARAMETER;
    if (!vdisks[idx].used) return EFI_NOT_FOUND;
    if (vdisks[idx].fh) return EFI_SUCCESS;
    return uefi_call_wrapper(gRoot->Open, 5, gRoot, &vdisks[idx].fh, vdisks[idx].name,
                             EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
}

// ======= Инициализация виртуальных дисков (disk.dat или fscs.bin) =======
EFI_STATUS ide_fat32_init(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) {
    EFI_STATUS st = ensure_root(ImageHandle, ST);
    if (EFI_ERROR(st)) return st;

    // init array
    for (int i = 0; i < MAX_DISKS; ++i) {
        vdisks[i].used = FALSE;
        vdisks[i].fh = NULL;
        vdisks[i].size_bytes = 0;
        vdisks[i].sectors = 0;
        SetMem(vdisks[i].name, sizeof(vdisks[i].name), 0);
    }

    CHAR8 *txt = NULL;
    UINTN txtsz = 0;
    st = read_entire_file_ascii(DISK_DAT_NAME, &txt, &txtsz);
    if (EFI_ERROR(st) || txtsz == 0) {
        // нет disk.dat — создаём дефолтный диск 0
        StrCpy(vdisks[0].name, DEFAULT_DISK_NAME);
        vdisks[0].sectors = (UINT32)DEFAULT_SECTORS;
        vdisks[0].size_bytes = (UINT64)vdisks[0].sectors * SECTOR_SIZE;
        vdisks[0].used = TRUE;

        ensure_backing_file_size(vdisks[0].name, vdisks[0].size_bytes);
        open_backing_handle(0);
        current_drive = 0;
        if (txt) FreePool(txt);
        return EFI_SUCCESS;
    }

    // Парсер простых строк "id filename sectors"
    CHAR8 *p = txt;
    while (*p) {
        // пропустить пробелы и пустые строки
        while (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;

        // id
        int id = 0;
        if (*p < '0' || *p > '9') break;
        while (*p >= '0' && *p <= '9') {
            id = id * 10 + (*p - '0'); p++;
        }
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        // filename (until space)
        CHAR8 fname8[120]; UINTN fi = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && fi < sizeof(fname8)-1) {
            fname8[fi++] = *p++; 
        }
        fname8[fi] = 0;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        // sectors (digits)
        unsigned long long secs = 0;
        if (*p < '0' || *p > '9') break;
        while (*p >= '0' && *p <= '9') { secs = secs * 10 + (*p - '0'); p++; }
        // skip to end of line
        while (*p && *p != '\n' && *p != '\r') p++;

        if (id >= 0 && id < MAX_DISKS) {
            AsciiStrToUnicodeStr(fname8, vdisks[id].name);
            vdisks[id].sectors = (UINT32)secs;
            vdisks[id].size_bytes = (UINT64)secs * SECTOR_SIZE;
            vdisks[id].used = TRUE;
            ensure_backing_file_size(vdisks[id].name, vdisks[id].size_bytes);
            open_backing_handle(id);
        }
    }

    FreePool(txt);
    // current -> first used
    for (int i = 0; i < MAX_DISKS; ++i) if (vdisks[i].used) { current_drive = (UINT8)i; break; }

    return EFI_SUCCESS;
}

// Удобный рантайм-инициализатор: создать диск в памяти (вызов из efi_main)
void init_virtual_disk(UINT64 bytes) {
    // вызывается после ide_fat32_init, но если нет — просто создаёт файл fscs.bin
    if (!gRoot) return;
    StrCpy(vdisks[0].name, DEFAULT_DISK_NAME);
    vdisks[0].sectors = (UINT32)(bytes / SECTOR_SIZE);
    vdisks[0].size_bytes = bytes;
    vdisks[0].used = TRUE;
    ensure_backing_file_size(vdisks[0].name, vdisks[0].size_bytes);
    open_backing_handle(0);
    current_drive = 0;
}

// Выбор диска
void ide_select_drive(UINT8 drive) {
    if (drive < MAX_DISKS && vdisks[drive].used) current_drive = drive;
}

// Подготовка R/W (в эмуляции просто проверка)
static int ide_setup_rw(uint32_t lba, uint8_t count) {
    VDISK *d = &vdisks[current_drive];
    if (!d->used) return VDISK_ERR_NODEV;
    if (lba >= d->sectors) return VDISK_ERR_IO;
    if (!d->fh) {
        if (EFI_ERROR(open_backing_handle(current_drive))) return VDISK_ERR_NODEV;
    }
    (void)count;
    return VDISK_ERR_OK;
}

// Чтение сектора (байтовый буфер)
int ide_read_sector(uint32_t lba, UINT8 *buffer) {
    if (!buffer) return VDISK_ERR_PARAM;
    int rc = ide_setup_rw(lba, 1);
    if (rc < 0) return rc;
    VDISK *d = &vdisks[current_drive];
    EFI_STATUS st = uefi_call_wrapper(d->fh->SetPosition, 2, d->fh, (UINT64)lba * SECTOR_SIZE);
    if (EFI_ERROR(st)) return VDISK_ERR_IO;
    UINTN size = SECTOR_SIZE;
    st = uefi_call_wrapper(d->fh->Read, 3, d->fh, &size, buffer);
    if (EFI_ERROR(st)) return VDISK_ERR_IO;
    if (size < SECTOR_SIZE) SetMem(buffer + size, SECTOR_SIZE - size, 0);
    return VDISK_ERR_OK;
}

// Запись сектора
int ide_write_sector(uint32_t lba, const UINT8 *buffer) {
    if (!buffer) return VDISK_ERR_PARAM;
    int rc = ide_setup_rw(lba, 1);
    if (rc < 0) return rc;
    VDISK *d = &vdisks[current_drive];
    EFI_STATUS st = uefi_call_wrapper(d->fh->SetPosition, 2, d->fh, (UINT64)lba * SECTOR_SIZE);
    if (EFI_ERROR(st)) return VDISK_ERR_IO;
    UINTN size = SECTOR_SIZE;
    st = uefi_call_wrapper(d->fh->Write, 3, d->fh, &size, (VOID*)buffer);
    if (EFI_ERROR(st) || size != SECTOR_SIZE) return VDISK_ERR_IO;
    return VDISK_ERR_OK;
}

// IDENTIFY (заполняем 256 слов; слова 60/61 = количество секторов)
int ide_identify(uint16_t *buffer256) {
    if (!buffer256) return VDISK_ERR_PARAM;
    VDISK *d = &vdisks[current_drive];
    if (!d->used) return VDISK_ERR_NODEV;
    for (int i = 0; i < 256; ++i) buffer256[i] = 0;
    // актуализируем размер через GetInfo
    if (d->fh) {
        EFI_GUID fileInfoGuid = EFI_FILE_INFO_ID;
        UINTN info_size = 0;
        EFI_FILE_INFO *info;
        EFI_STATUS st = uefi_call_wrapper(d->fh->GetInfo, 4, d->fh, &fileInfoGuid, &info_size, NULL);
        if (st == EFI_BUFFER_TOO_SMALL) {
            info = AllocatePool(info_size);
            if (info) {
                if (!EFI_ERROR(uefi_call_wrapper(d->fh->GetInfo, 4, d->fh, &fileInfoGuid, &info_size, info))) {
                    d->size_bytes = info->FileSize;
                    d->sectors = (UINT32)(d->size_bytes / SECTOR_SIZE);
                }
                FreePool(info);
            }
        }
    }
    uint32_t s32 = d->sectors;
    buffer256[60] = (uint16_t)(s32 & 0xFFFF);
    buffer256[61] = (uint16_t)((s32 >> 16) & 0xFFFF);
    // Записываем имя в words 27..46 (ASCII парами) — не обязательно строго правильный формат
    CHAR16 tmp[41]; StrnCpy(tmp, d->name, 40);
    int wi = 27;
    for (int i = 0; i < 40 && tmp[i]; i += 2) {
        CHAR16 a = tmp[i];
        CHAR16 b = tmp[i+1] ? tmp[i+1] : ' ';
        buffer256[wi++] = (uint16_t)(((a & 0xFF) << 8) | (b & 0xFF));
    }
    return VDISK_ERR_OK;
}

uint32_t ide_get_sector_count(void) {
    VDISK *d = &vdisks[current_drive];
    if (!d->used) return 0;
    return d->sectors;
}

uint8_t ide_read_byte(uint32_t index) {
    uint8_t buf[SECTOR_SIZE];
    uint32_t sector = index / SECTOR_SIZE;
    uint32_t offset = index % SECTOR_SIZE;
    if (ide_read_sector(sector, buf) != VDISK_ERR_OK) return 0xFF;
    return buf[offset];
}

void ide_write_byte(uint32_t index, uint8_t value) {
    uint8_t buf[SECTOR_SIZE];
    uint32_t sector = index / SECTOR_SIZE;
    uint32_t offset = index % SECTOR_SIZE;
    if (ide_read_sector(sector, buf) != VDISK_ERR_OK) return;
    buf[offset] = value;
    (void)ide_write_sector(sector, buf);
}

#endif // VIRTUAL_DISK_DRIVER_H
