#include "../drivers/fat32.h"

// ===== Текущий диск, путь и массив дисков =====
EFI_FILE_PROTOCOL *FAT32_CWD = NULL;                     // текущий открытый каталог (EFI_FILE_PROTOCOL*)
CHAR16 FAT32_CurrentPath[MAX_PATH_LEN] = L"\\";          // путь внутри текущего диска, например "\EFI\BOOT"
CHAR16 FAT32_CurrentDisk = L'A';                         // текущая буква диска, по умолчанию A

EFI_FILE_PROTOCOL *FAT32_Disks[MAX_DISKS];               // корни (root handles) зарегистрированных дисков
CHAR16 FAT32_DiskLetters[MAX_DISKS];                     // буквы для дисков
UINTN FAT32_DiskCount = 0;

// ===== Вспомогательные функции =====
// Разбить строку командной (как у тебя)
UINTN FAT32_SplitLine(CHAR16 *line, CHAR16 *args[], UINTN max_args) {
    UINTN argc = 0;
    CHAR16 *ptr = line;
    while (*ptr && argc < max_args) {
        while (*ptr == L' ') ptr++;
        if (*ptr == 0) break;
        args[argc++] = ptr;
        while (*ptr && *ptr != L' ') ptr++;
        if (*ptr == L' ') *ptr++ = 0;
    }
    return argc;
}

// Получить "полный" путь вида "A:\path\to\dir" (возвращает статический буфер)
CHAR16* FAT32_GetFullPathString() {
    static CHAR16 out[MAX_PATH_LEN + 4];
    out[0] = FAT32_CurrentDisk;
    out[1] = L':';
    out[2] = L'\0';
    // Если CurrentPath == "\" то получится "A:\"
    if (FAT32_CurrentPath[0] == L'\\') {
        // склеиваем
        StrCat(out, FAT32_CurrentPath);
    } else {
        // на всякий случай
        StrCat(out, L"\\");
        StrCat(out, FAT32_CurrentPath);
    }
    return out;
}

// Найти индекс диска по букве, -1 если не найден
INTN FAT32_FindDiskIndex(CHAR16 letter) {
    for (UINTN i = 0; i < FAT32_DiskCount; i++) {
        if (FAT32_DiskLetters[i] == letter) return (INTN)i;
    }
    return -1;
}

// Убрать ведущий символ '\' если есть, возвращает pointer to first non-backslash
static CHAR16* strip_leading_backslash(CHAR16 *p) {
    if (!p) return p;
    while (*p == L'\\') p++;
    return p;
}
void Fat32_RegisterrsDisk() {
        UINTN HandleCount;
            EFI_HANDLE *Handles;
            EFI_STATUS Status;

            // Ищем все файловые системы
            Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5,
                                    ByProtocol,
                                    &gEfiSimpleFileSystemProtocolGuid,
                                    NULL,
                                    &HandleCount,
                                    &Handles);

            if (!EFI_ERROR(Status)) {
                for (UINTN i = 0; i < HandleCount; i++) {
                    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
                    EFI_FILE_PROTOCOL *Root;

                    Status = uefi_call_wrapper(BS->HandleProtocol, 3,
                                            Handles[i],
                                            &gEfiSimpleFileSystemProtocolGuid,
                                            (VOID**)&Fs);
                    if (EFI_ERROR(Status)) continue;

                    Status = uefi_call_wrapper(Fs->OpenVolume, 2, Fs, &Root);
                    if (EFI_ERROR(Status)) continue;

                    // Первый диск уже зарегистрирован как A,
                    // второй пойдет в B, третий в C и т.д.
                     if (i == 0) {
                        FAT32_RegisterDisk(L'A', Root);
                    }if (i == 1) {
                        FAT32_RegisterDisk(L'B', Root);
                    } if (i == 2) {
                        FAT32_RegisterDisk(L'C', Root);
                    } if (i == 3) {
                        FAT32_RegisterDisk(L'D', Root);
                    }if (i == 4) {
                        FAT32_RegisterDisk(L'E', Root);
                    }if (i == 5) {
                        FAT32_RegisterDisk(L'F', Root);
                    }if (i == 6) {
                        FAT32_RegisterDisk(L'G', Root);
                    }if (i == 7) {
                        FAT32_RegisterDisk(L'H', Root);
                    }if (i == 8) {
                        FAT32_RegisterDisk(L'I', Root);
                    }if (i == 9) {
                        FAT32_RegisterDisk(L'J', Root);
                    }if (i == 10) {
                        FAT32_RegisterDisk(L'K', Root);
                    }if (i == 11) {
                        FAT32_RegisterDisk(L'L', Root);
                    }if (i == 12) {
                        FAT32_RegisterDisk(L'M', Root);
                    }if (i == 13) {
                        FAT32_RegisterDisk(L'N', Root);
                    }if (i == 14) {
                        FAT32_RegisterDisk(L'O', Root);
                    }if (i == 15) {
                        FAT32_RegisterDisk(L'P', Root);
                    }if (i == 16) {
                        FAT32_RegisterDisk(L'Q', Root);
                    }if (i == 17) {
                        FAT32_RegisterDisk(L'R', Root);
                    }if (i == 18) {
                        FAT32_RegisterDisk(L'S', Root);
                    }if (i == 19) {
                        FAT32_RegisterDisk(L'T', Root);
                    }if (i == 20) {
                        FAT32_RegisterDisk(L'U', Root);
                    }if (i == 21) {
                        FAT32_RegisterDisk(L'V', Root);
                    }if (i == 22) {
                        FAT32_RegisterDisk(L'W', Root);
                    }if (i == 23) {
                        FAT32_RegisterDisk(L'X', Root);
                    }if (i == 24) {
                        FAT32_RegisterDisk(L'Y', Root);
                    }if (i == 25) {
                        FAT32_RegisterDisk(L'Z', Root);
            }
        }
    }
}
// ===== Работа с дисками =====
// Регистрируем диск: передаём букву и корневой EFI_FILE_PROTOCOL* (root)
void FAT32_RegisterDisk(CHAR16 letter, EFI_FILE_PROTOCOL *root) {
    if (FAT32_DiskCount >= MAX_DISKS) return;

    // Проверяем, не зарегистрирован ли этот диск уже
    for (UINTN i = 0; i < FAT32_DiskCount; i++) {
        if (FAT32_DiskLetters[i] == letter) {
            return; // Уже существует
        }
    }

    // Регистрируем новый диск
    FAT32_Disks[FAT32_DiskCount] = root;
    FAT32_DiskLetters[FAT32_DiskCount] = letter;
    FAT32_DiskCount++;

    // Если это первый диск — делаем его текущим
    if (FAT32_CWD == NULL) {
        FAT32_CWD = root;
        FAT32_CurrentDisk = letter;
        StrCpy(FAT32_CurrentPath, L"\\");
    }
}


// Смена текущего диска (буква). Закрывает старый CWD, устанавливает новый root и сбрасывает путь.
EFI_STATUS FAT32_ChangeDisk(CHAR16 letter) {
    INTN idx = FAT32_FindDiskIndex(letter);
    if (idx < 0) return EFI_NOT_FOUND;

    // Закрываем текущий CWD, если он не равен корню нового диска (и не NULL)
    if (FAT32_CWD && FAT32_CWD != FAT32_Disks[idx]) {
        uefi_call_wrapper(FAT32_CWD->Close, 1, FAT32_CWD);
    }

    FAT32_CWD = FAT32_Disks[idx];
    FAT32_CurrentDisk = letter;
    StrCpy(FAT32_CurrentPath, L"\\");
    return EFI_SUCCESS;
}

// ===== Получение корня для текущего ImageHandle (как у тебя было) =====
EFI_FILE_PROTOCOL* FAT32_GetRoot(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST) {
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_STATUS s = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3,
                                     ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&loaded_image);
    if (EFI_ERROR(s)) return NULL;

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    s = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3,
                          loaded_image->DeviceHandle,
                          &gEfiSimpleFileSystemProtocolGuid,
                          (void**)&fs);
    if (EFI_ERROR(s)) return NULL;

    EFI_FILE_PROTOCOL *root;
    s = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(s)) return NULL;
    return root;
}
struct EC16 FAT32_ListDisks() {
    struct EC16 result;
    result.Status = EFI_SUCCESS;

    // Считаем общую длину для буфера
    UINTN total_len = 0;
    for (UINTN i = 0; i < FAT32_DiskCount; i++) {
        total_len += 16; // запас на "A: [root]\n"
    }

    CHAR16 *buffer = AllocatePool(total_len * sizeof(CHAR16));
    if (!buffer) {
        result.Status = EFI_OUT_OF_RESOURCES;
        result.Message = L"Out of memory\n";
        return result;
    }
    buffer[0] = 0;

    for (UINTN i = 0; i < FAT32_DiskCount; i++) {
        CHAR16 line[32];
        SPrint(line, sizeof(line), L"%c: [root]\n", FAT32_DiskLetters[i]);
        StrCat(buffer, line);
    }

    result.Message = buffer;
    return result;
}

// ===== Открытие каталога =====
// Открыть каталог path относительный к указанному root_handle.
// Если root_handle == NULL -> используем FAT32_CWD как базу.
EFI_STATUS FAT32_OpenRelative(EFI_FILE_PROTOCOL *root_handle, CHAR16 *path, EFI_FILE_PROTOCOL **out_dir) {
    EFI_FILE_PROTOCOL *base = root_handle ? root_handle : FAT32_CWD;
    if (!base) return EFI_INVALID_PARAMETER;

    EFI_STATUS status;
    EFI_FILE_PROTOCOL *dir;
    // strip leading '\' если есть (если путь абсолютный, caller должен позаботиться)
    CHAR16 *p = path;
    // Note: UEFI Open accepts L"Name" segments; passing backslashes may or may not be treated;
    // We'll pass path as-is; many EFI implementations accept 'Dir\Sub' as path.
    status = uefi_call_wrapper(base->Open, 5, base, &dir, p, EFI_FILE_MODE_READ, EFI_FILE_DIRECTORY);
    if (EFI_ERROR(status)) return status;
    *out_dir = dir;
    return EFI_SUCCESS;
}
// Предполагается, что где-то есть такие глобальные переменные:
// static CHAR16 FAT32_CurrentDisk = L'A';
// static CHAR16 FAT32_CurrentPath[256] = L"\\";

CHAR16* FAT32_PrintCurrentPath() {
    static CHAR16 buffer[512]; // Буфер для готовой строки
    SPrint(buffer, sizeof(buffer), L"%c:%s", FAT32_CurrentDisk, FAT32_CurrentPath);
    return buffer;
}

// ===== Парсер пути Windows-стиля =====
// Разбирает входную строку path_src. Поддерживает:
//  - "C:\foo\bar"  => диск C, абсолютный путь
//  - "C:foo\bar"   => диск C, относительный к текущему каталогу этого диска
//  - "\foo\bar"    => абсолютный по текущему диску (т.е. A:\foo\bar)
//  - "foo\bar"     => относительный к текущему каталогу на текущем диске
//
// Возвращает EFI_STATUS и заполняет:
//   outDiskLetter (если != 0 — означает, что в строке была буква диска и мы должны её использовать)
//   outIsAbsolute (TRUE если абсолютный: после буквы был ':' и '\' или первый символ был '\')
//   outPathPtr — pointer to path portion to use with Open (may skip "C:" or leading '\')
EFI_STATUS FAT32_ParsePath(CHAR16 *path_src, CHAR16 *outDiskLetter, BOOLEAN *outIsAbsolute, CHAR16 **outPathPtr) {
    if (!path_src || !outDiskLetter || !outIsAbsolute || !outPathPtr) return EFI_INVALID_PARAMETER;

    *outDiskLetter = 0;
    *outIsAbsolute = FALSE;
    *outPathPtr = path_src;

    // Trim leading spaces
    while (*path_src == L' ') path_src++;

    // Check for "X:" at beginning (letter + ':')
    if (((path_src[0] >= L'A' && path_src[0] <= L'Z') || (path_src[0] >= L'a' && path_src[0] <= L'z')) && path_src[1] == L':') {
        CHAR16 letter = path_src[0];
        // Normalize to uppercase
        if (letter >= L'a' && letter <= L'z') letter = letter - (L'a' - L'A');
        *outDiskLetter = letter;
        // skip "C:"
        path_src += 2;
        // if next char is backslash, it's absolute
        if (*path_src == L'\\') {
            *outIsAbsolute = TRUE;
            // skip leading backslashes for path pointer
            *outPathPtr = path_src + 1;
        } else {
            // relative to that disk's current dir (or to root if that disk not current)
            *outIsAbsolute = FALSE;
            *outPathPtr = path_src;
        }
        return EFI_SUCCESS;
    }

    // If path starts with backslash -> absolute on current disk
    if (path_src[0] == L'\\') {
        *outIsAbsolute = TRUE;
        *outPathPtr = path_src + 1;
        return EFI_SUCCESS;
    }

    // else relative path, current disk
    *outIsAbsolute = FALSE;
    *outPathPtr = path_src;
    return EFI_SUCCESS;
}

// ===== Нормализация и комбинация сегментов: build new FAT32_CurrentPath =====
// BuildNormalizedFromSegments: helper that given current path and a relative path with components,
// applies "." and ".." and produces a normalized path (always starting with '\').
static void BuildNormalizedPath(CHAR16 *dest, const CHAR16 *basePath, const CHAR16 *relPath) {
    // We'll implement a simple segments stack using pointers into a local buffer.
    // dest must be at least MAX_PATH_LEN.
    CHAR16 work[MAX_PATH_LEN];
    work[0] = L'\0';

    // Start from basePath if provided (should start with '\')
    if (basePath && basePath[0] == L'\\') {
        // copy base but without trailing backslash (unless root only)
        StrCpy(work, basePath);
        UINTN len = StrLen(work);
        if (len > 1 && work[len - 1] == L'\\') {
            work[len - 1] = L'\0';
        }
    } else {
        StrCpy(work, L"\\");
    }

    // If relPath is empty -> dest = work
    if (!relPath || *relPath == L'\0') {
        // ensure leading backslash
        if (work[0] != L'\\') {
            // build "\"
            dest[0] = L'\\';
            dest[1] = L'\0';
            return;
        } else {
            StrCpy(dest, work);
            return;
        }
    }

    // We'll create an array of segment pointers for work stack
    CHAR16 segments[64][128]; // safe small stack (64 segments, each up to 127 chars)
    UINTN segc = 0;

    // push base path segments (skip leading '\')
    UINTN i = 0;
    if (work[0] == L'\\') i = 1;
    UINTN wlen = StrLen(work);
    while (i < wlen) {
        // read segment
        CHAR16 segbuf[128];
        UINTN p = 0;
        while (i < wlen && work[i] != L'\\' && p < 127) {
            segbuf[p++] = work[i++];
        }
        segbuf[p] = L'\0';
        if (p > 0 && segc < 64) {
            StrCpy(segments[segc++], segbuf);
        }
        if (i < wlen && work[i] == L'\\') i++;
    }

    // parse relPath segments separated by '\'
    i = 0;
    UINTN rlen = StrLen(relPath);
    while (i < rlen) {
        CHAR16 segbuf[128];
        UINTN p = 0;
        while (i < rlen && relPath[i] != L'\\' && p < 127) {
            segbuf[p++] = relPath[i++];
        }
        segbuf[p] = L'\0';
        if (p == 0) { // consecutive backslashes
            if (i < rlen && relPath[i] == L'\\') i++;
            continue;
        }
        if (StrCmp(segbuf, L".") == 0) {
            // no-op
        } else if (StrCmp(segbuf, L"..") == 0) {
            if (segc > 0) segc--;
        } else {
            if (segc < 64) StrCpy(segments[segc++], segbuf);
        }
        if (i < rlen && relPath[i] == L'\\') i++;
    }

    // build dest: start with '\'
    dest[0] = L'\\';
    dest[1] = L'\0';
    if (segc == 0) {
        // root
        return;
    }

    for (UINTN s = 0; s < segc; s++) {
        StrCat(dest, segments[s]);
        if (s + 1 < segc) StrCat(dest, L"\\");
    }
}

// ===== Основная функция: смена директории Windows-стиль =====
// Поддерживает:
//  - "C:\abs\path"  -> меняет диск на C и переходит в \abs\path
//  - "C:rel\path"   -> меняет диск на C и переходит в current_of_C + rel\path
//  - "\abs\path"    -> на текущем диске перейти в \abs\path
//  - "rel\path"     -> на текущем диске перейти в current + rel\path
//  - "cd .." работает корректно
EFI_STATUS FAT32_ChangeDirEx(CHAR16 *inputPath) {
    if (!inputPath) return EFI_INVALID_PARAMETER;

    CHAR16 diskLetter = 0;
    BOOLEAN isAbsolute = FALSE;
    CHAR16 *pathPart = NULL;

    EFI_STATUS s = FAT32_ParsePath(inputPath, &diskLetter, &isAbsolute, &pathPart);
    if (EFI_ERROR(s)) return s;

    // Если в пути указана буква диска — переключаемся на неё
    if (diskLetter) {
        EFI_STATUS cs = FAT32_ChangeDisk(diskLetter);
        if (EFI_ERROR(cs)) return cs;
    }

    // Получили базовый root для операции (текущий диск)
    INTN curIndex = FAT32_FindDiskIndex(FAT32_CurrentDisk);
    EFI_FILE_PROTOCOL *root_handle = NULL;
    if (curIndex >= 0) root_handle = FAT32_Disks[curIndex];
    else root_handle = FAT32_CWD; // fallback

    // Если абсолютный — открываем от корня диска
    if (isAbsolute) {
        // pathPart указывает на часть без ведущего '\' (strip_leading_backslash done in ParsePath)
        EFI_FILE_PROTOCOL *newdir;
        // open using root_handle as base (which is root)
        s = FAT32_OpenRelative(root_handle, pathPart, &newdir);
        if (EFI_ERROR(s)) return s;

        // close old cwd if not root_handle (and not NULL)
        if (FAT32_CWD && FAT32_CWD != root_handle) uefi_call_wrapper(FAT32_CWD->Close, 1, FAT32_CWD);
        FAT32_CWD = newdir;

        // normalize and set FAT32_CurrentPath to \... (build with leading backslash)
        CHAR16 norm[MAX_PATH_LEN];
        BuildNormalizedPath(norm, L"\\", pathPart);
        StrCpy(FAT32_CurrentPath, norm);
        return EFI_SUCCESS;
    }

    // Относительный путь: open relative to current CWD
    EFI_FILE_PROTOCOL *newdir;
    s = FAT32_OpenRelative(FAT32_CWD, pathPart, &newdir);
    if (EFI_ERROR(s)) return s;

    // close old cwd if it's not a registered disk root (and not NULL)
    INTN rootidx = FAT32_FindDiskIndex(FAT32_CurrentDisk);
    EFI_FILE_PROTOCOL *registeredRoot = (rootidx >= 0) ? FAT32_Disks[rootidx] : NULL;
    if (FAT32_CWD && FAT32_CWD != registeredRoot) uefi_call_wrapper(FAT32_CWD->Close, 1, FAT32_CWD);
    FAT32_CWD = newdir;

    // Build new normalized path from previous path and relative pathPart
    CHAR16 norm[MAX_PATH_LEN];
    BuildNormalizedPath(norm, FAT32_CurrentPath, pathPart);
    StrCpy(FAT32_CurrentPath, norm);
    return EFI_SUCCESS;
}
struct EC16 FAT32_ListDir() {
    struct EC16 result;
    result.Status = EFI_SUCCESS;
    result.Message = NULL;

    if (!FAT32_CWD) {
        result.Status = EFI_NOT_READY;
        return result;
    }

    EFI_FILE_PROTOCOL *dir;
    EFI_STATUS status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &dir, L".",
                                          EFI_FILE_MODE_READ, EFI_FILE_DIRECTORY);
    if (EFI_ERROR(status)) {
        result.Status = status;
        return result;
    }

    UINTN buf_size = sizeof(EFI_FILE_INFO) + 512;
    EFI_FILE_INFO *info = AllocatePool(buf_size);
    if (!info) {
        uefi_call_wrapper(dir->Close, 1, dir);
        result.Status = EFI_OUT_OF_RESOURCES;
        return result;
    }

    // dynamic out buffer (начало)
    CHAR16 *outbuf = AllocatePool(sizeof(CHAR16) * 2);
    if (!outbuf) {
        FreePool(info);
        uefi_call_wrapper(dir->Close, 1, dir);
        result.Status = EFI_OUT_OF_RESOURCES;
        return result;
    }
    outbuf[0] = L'\0';

    while (TRUE) {
        buf_size = sizeof(EFI_FILE_INFO) + 512;
        status = uefi_call_wrapper(dir->Read, 3, dir, &buf_size, info);
        if (EFI_ERROR(status)) break;
        if (buf_size == 0) break; // конец каталога

        // пропускаем "." и ".."
        if ((StrCmp(info->FileName, L".") == 0) ||
            (StrCmp(info->FileName, L"..") == 0)) {
            continue;
        }

        UINTN old_len = StrLen(outbuf);
        UINTN add_len = StrLen(L"dir-") + StrLen(info->FileName) + 1; // +1 на \n
        CHAR16 *newbuf = AllocatePool(sizeof(CHAR16) * (old_len + add_len + 1)); // +1 под \0
        if (!newbuf) {
            FreePool(outbuf);
            FreePool(info);
            uefi_call_wrapper(dir->Close, 1, dir);
            result.Status = EFI_OUT_OF_RESOURCES;
            return result;
        }

        newbuf[0] = L'\0';
        StrCpy(newbuf, outbuf);
        FreePool(outbuf);

        if (info->Attribute & EFI_FILE_DIRECTORY) {
            StrCat(newbuf, L"<dir>  ");
        } else {
            StrCat(newbuf, L"<file> ");
        }

        StrCat(newbuf, info->FileName);
        StrCat(newbuf, L"\n");

        outbuf = newbuf;
    }
    FreePool(info);
    uefi_call_wrapper(dir->Close, 1, dir);

    result.Message = outbuf;
    result.Status = EFI_SUCCESS;
    return result;
}
// ===== Создание/удаление файлов и директорий – используют FAT32_CWD =====
EFI_STATUS FAT32_CreateDir(CHAR16 *name) {
    if (!FAT32_CWD) return EFI_NOT_READY;
    EFI_FILE_PROTOCOL *dir;
    EFI_STATUS status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &dir, name,
                        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                        EFI_FILE_DIRECTORY);
    if (!EFI_ERROR(status)) uefi_call_wrapper(dir->Close, 1, dir);
    return status;
}

EFI_STATUS FAT32_DeleteDir(CHAR16 *name) {
    if (!FAT32_CWD) return EFI_NOT_READY;
    EFI_FILE_PROTOCOL *dir;
    EFI_STATUS status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &dir, name,
                                          EFI_FILE_MODE_READ, EFI_FILE_DIRECTORY);
    if (EFI_ERROR(status)) return status;
    return uefi_call_wrapper(dir->Delete, 1, dir);
}

EFI_STATUS FAT32_CreateFile(CHAR16 *name) {
    if (!FAT32_CWD) return EFI_NOT_READY;
    EFI_FILE_PROTOCOL *file;
    EFI_STATUS status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &file, name,
                        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (!EFI_ERROR(status)) uefi_call_wrapper(file->Close, 1, file);
    return status;
}

EFI_STATUS FAT32_DeleteFile(CHAR16 *name) {
    if (!FAT32_CWD) return EFI_NOT_READY;
    EFI_FILE_PROTOCOL *file;
    EFI_STATUS status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &file, name,
                                          EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) return status;
    return uefi_call_wrapper(file->Delete, 1, file);
}


struct EC16 FAT32_ReadFile(CHAR16 *filename) {
    struct EC16 result;
    result.Status = EFI_NOT_READY;
    result.Message = NULL;
    result.FileSize = 0;

    if (!FAT32_CWD) return result;

    EFI_FILE_PROTOCOL *file;
    EFI_STATUS status = uefi_call_wrapper(
        FAT32_CWD->Open, 5,
        FAT32_CWD,
        &file,
        filename,
        EFI_FILE_MODE_READ,
        0
    );

    if (EFI_ERROR(status)) {
        result.Status = status;
        return result;
    }

    //
    // Определяем реальный размер файла через GetInfo
    //
    EFI_FILE_INFO *info;
    UINTN info_size = sizeof(EFI_FILE_INFO) + 200;
    info = AllocatePool(info_size);

    status = uefi_call_wrapper(
        file->GetInfo, 4,
        file,
        &gEfiFileInfoGuid,
        &info_size,
        info
    );

    if (EFI_ERROR(status)) {
        uefi_call_wrapper(file->Close, 1, file);
        FreePool(info);
        result.Status = status;
        return result;
    }

    UINTN file_size = info->FileSize;
    FreePool(info);

    //
    // Выделяем буфер под файл
    //
    UINT8 *buf = AllocatePool(file_size);
    if (!buf) {
        uefi_call_wrapper(file->Close, 1, file);
        result.Status = EFI_OUT_OF_RESOURCES;
        return result;
    }

    //
    // Читаем весь файл как байты
    //
    status = uefi_call_wrapper(
        file->Read, 3,
        file,
        &file_size,
        buf
    );

    uefi_call_wrapper(file->Close, 1, file);

    if (EFI_ERROR(status)) {
        FreePool(buf);
        result.Status = status;
        return result;
    }

    // Заполняем результат
    result.Status = EFI_SUCCESS;
    result.Message = buf;
    result.FileSize = file_size;

    return result;
}
EFI_STATUS FAT32_WriteFile(CHAR16 *filename, UINT16 *data, UINTN len) {
    if (!FAT32_CWD) return EFI_NOT_READY;
    EFI_FILE_PROTOCOL *file;
    EFI_STATUS status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &file, filename,
                        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(status)) return status;
    UINTN size = len * sizeof(UINT16);
    uefi_call_wrapper(file->Write, 3, file, &size, data);
    uefi_call_wrapper(file->Close, 1, file);
    return EFI_SUCCESS;
}
// Простое копирование файла (не рекурсивно): src -> dest (в текущем CWD).
EFI_STATUS FAT32_CopyFile(CHAR16 *src, CHAR16 *dest) {
    if (!FAT32_CWD) return EFI_NOT_READY;
    EFI_FILE_PROTOCOL *srcf;
    EFI_FILE_PROTOCOL *dstf;
    EFI_STATUS status;

    status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &srcf, src, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) return status;

    status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &dstf, dest,
                               EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(srcf->Close, 1, srcf);
        return status;
    }

    // буфер 8KB
    UINTN chunk = 8 * 1024;
    VOID *buf = AllocatePool(chunk);
    if (!buf) { uefi_call_wrapper(srcf->Close,1,srcf); uefi_call_wrapper(dstf->Close,1,dstf); return EFI_OUT_OF_RESOURCES; }

    while (TRUE) {
        UINTN read = chunk;
        status = uefi_call_wrapper(srcf->Read, 3, srcf, &read, buf);
        if (EFI_ERROR(status)) break;
        if (read == 0) { status = EFI_SUCCESS; break; } // EOF
        UINTN written = read;
        status = uefi_call_wrapper(dstf->Write, 3, dstf, &written, buf);
        if (EFI_ERROR(status)) break;
    }

    FreePool(buf);
    uefi_call_wrapper(srcf->Close, 1, srcf);
    uefi_call_wrapper(dstf->Close, 1, dstf);
    return status;
}

EFI_STATUS FAT32_DeleteEntry(CHAR16 *name) {
    if (!FAT32_CWD) return EFI_NOT_READY;

    EFI_FILE_PROTOCOL *f;
    EFI_STATUS status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &f, name, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) return status;

    // Получим инфо, чтобы понять, файл это или папка
    UINTN info_size = sizeof(EFI_FILE_INFO) + 512;
    EFI_FILE_INFO *info = AllocatePool(info_size);
    if (!info) { uefi_call_wrapper(f->Close,1,f); return EFI_OUT_OF_RESOURCES; }

    status = uefi_call_wrapper(f->GetInfo, 4, f, &gEfiFileInfoGuid, &info_size, info);
    if (EFI_ERROR(status)) {
        FreePool(info);
        uefi_call_wrapper(f->Close,1,f);
        return status;
    }

    // Вызов Delete на handle'е - удаляет и закрывает handle (UEFI spec).
    status = uefi_call_wrapper(f->Delete, 1, f);

    FreePool(info);
    return status;
}

EFI_STATUS FAT32_MoveFile(CHAR16 *src, CHAR16 *dest) {
    EFI_STATUS s = FAT32_CopyFile(src, dest);
    if (EFI_ERROR(s)) return s;
    s = FAT32_DeleteEntry(src);
    return s;
}
EFI_STATUS FAT32_AppendFile(CHAR16 *filename, UINT16 *data, UINTN len) {
    if (!FAT32_CWD) return EFI_NOT_READY;
    EFI_FILE_PROTOCOL *file;
    EFI_STATUS status = uefi_call_wrapper(FAT32_CWD->Open, 5, FAT32_CWD, &file, filename,
                                          EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (EFI_ERROR(status)) return status;

    uefi_call_wrapper(file->SetPosition, 2, file, 0xFFFFFFFFFFFFFFFFULL);
    UINTN size = len * sizeof(UINT16);
    uefi_call_wrapper(file->Write, 3, file, &size, data);
    uefi_call_wrapper(file->Close, 1, file);
    return EFI_SUCCESS;
}
EFI_STATUS FAT32_ReadFileByPath(CHAR16 *path_in, struct EC16 *out) {
    if (!path_in || !out) return EFI_INVALID_PARAMETER;

    out->Status = EFI_INVALID_PARAMETER;
    out->Message = NULL;
    out->FileSize = 0;

    CHAR16 pathbuf[MAX_PATH_LEN];
    if (StrLen(path_in) == 0 || StrLen(path_in) >= MAX_PATH_LEN) {
        out->Status = EFI_INVALID_PARAMETER;
        return EFI_INVALID_PARAMETER;
    }
    StrCpy(pathbuf, path_in);

    CHAR16 diskLetter = 0;
    BOOLEAN isAbsolute = FALSE;
    CHAR16 *pathPart = NULL;
    EFI_STATUS s = FAT32_ParsePath(pathbuf, &diskLetter, &isAbsolute, &pathPart);
    if (EFI_ERROR(s)) { out->Status = s; return s; }

    EFI_FILE_PROTOCOL *base = NULL;
    if (diskLetter) {
        INTN idx = FAT32_FindDiskIndex(diskLetter);
        if (idx < 0) { out->Status = EFI_NOT_FOUND; return EFI_NOT_FOUND; }
        base = FAT32_Disks[idx];
    } else {
        INTN idx = FAT32_FindDiskIndex(FAT32_CurrentDisk);
        base = (idx >= 0) ? FAT32_Disks[idx] : FAT32_CWD;
    }
    if (!base) { out->Status = EFI_NOT_READY; return EFI_NOT_READY; }

    EFI_FILE_PROTOCOL *fhandle = NULL;
    s = uefi_call_wrapper(base->Open, 5, base, &fhandle, pathPart, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(s)) { out->Status = s; return s; }

    UINTN info_size = sizeof(EFI_FILE_INFO) + 512;
    EFI_FILE_INFO *info = AllocatePool(info_size);
    if (!info) { uefi_call_wrapper(fhandle->Close, 1, fhandle); out->Status = EFI_OUT_OF_RESOURCES; return EFI_OUT_OF_RESOURCES; }

    s = uefi_call_wrapper(fhandle->GetInfo, 4, fhandle, &gEfiFileInfoGuid, &info_size, info);
    if (EFI_ERROR(s)) { FreePool(info); uefi_call_wrapper(fhandle->Close, 1, fhandle); out->Status = s; return s; }

    UINTN file_size = info->FileSize;
    FreePool(info);

    UINT8 *buf = AllocatePool(file_size);
    if (!buf) { uefi_call_wrapper(fhandle->Close, 1, fhandle); out->Status = EFI_OUT_OF_RESOURCES; return EFI_OUT_OF_RESOURCES; }

    UINTN read_size = file_size;
    s = uefi_call_wrapper(fhandle->Read, 3, fhandle, &read_size, buf);
    uefi_call_wrapper(fhandle->Close, 1, fhandle);

    if (EFI_ERROR(s)) { FreePool(buf); out->Status = s; return s; }

    out->Status = EFI_SUCCESS;
    out->Message = buf;
    out->FileSize = file_size;
    return EFI_SUCCESS;
}
struct EC16* FAT32_ListSimple(UINTN *SizeOut) {
    if (!FAT32_CWD) {
        *SizeOut = 0;
        return NULL;
    }

    EFI_STATUS Status;
    EFI_FILE_INFO *info;
    UINTN infoSize = sizeof(EFI_FILE_INFO) + 512;
    VOID *infoBuf = AllocatePool(infoSize);
    if (!infoBuf) {
        *SizeOut = 0;
        return NULL;
    }

    // Для подсчёта количества
    UINTN count = 0;

    // 1) Сначала считаем количество
    FAT32_CWD->SetPosition(FAT32_CWD, 0);
    while (TRUE) {
        infoSize = sizeof(EFI_FILE_INFO) + 512;
        Status = FAT32_CWD->Read(FAT32_CWD, &infoSize, infoBuf);
        if (EFI_ERROR(Status) || infoSize == 0) break;

        info = (EFI_FILE_INFO*)infoBuf;
        if (info->FileName[0] == 0) continue;
        if (StrCmp(info->FileName, L".") == 0) continue;
        if (StrCmp(info->FileName, L"..") == 0) continue;

        count++;
    }

    // выделяем массив
    struct EC16 *result = AllocatePool(count * sizeof(struct EC16));
    if (!result) {
        FreePool(infoBuf);
        *SizeOut = 0;
        return NULL;
    }

    // 2) заполняем массив
    FAT32_CWD->SetPosition(FAT32_CWD, 0);
    UINTN index = 0;

    while (TRUE) {
        infoSize = sizeof(EFI_FILE_INFO) + 512;
        Status = FAT32_CWD->Read(FAT32_CWD, &infoSize, infoBuf);
        if (EFI_ERROR(Status) || infoSize == 0) break;

        info = (EFI_FILE_INFO*)infoBuf;

        if (info->FileName[0] == 0) continue;
        if (StrCmp(info->FileName, L".") == 0) continue;
        if (StrCmp(info->FileName, L"..") == 0) continue;

        // Копируем имя файла
        UINTN len = (StrLen(info->FileName) + 1) * sizeof(CHAR16);
        CHAR16 *copy = AllocatePool(len);
        StrCpy(copy, info->FileName);

        result[index].Message = copy;
        index++;
    }

    FreePool(infoBuf);

    *SizeOut = count;
    return result;
}
