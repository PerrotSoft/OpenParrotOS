#include <efi.h>
#include <efilib.h>
#include "console_app.h"
#include "drivers/fat32.h"
#include "drivers/keyboard_driver.h"
#include "sys.h"
UINT32 color =White;
EFI_STATUS run_comsole(CHAR16 *line,EFI_HANDLE ImageHandle);
EFI_STATUS RunScriptFile(CHAR16 *path) {
    struct EC16 file = FAT32_ReadFile(path);
    if (EFI_ERROR(file.Status) || file.Message == NULL)
        return file.Status;

    CHAR16 *text = file.Message;
    UINTN len = StrLen(text);

    UINTN start = 0;

    while (start < len) {
        // Ищем конец строки
        UINTN end = start;
        while (end < len && text[end] != L'\n' && text[end] != L'\r')
            end++;

        // Выделяем строку
        UINTN lineLen = end - start;
        if (lineLen > 0) {
            CHAR16 *line = AllocateZeroPool((lineLen + 1) * sizeof(CHAR16));
            for (UINTN i = 0; i < lineLen; i++)
                line[i] = text[start + i];

            ExecuteConsoleCommand(line);  // ← твоя функция обработки команд
            FreePool(line);
        }

        // Переходим к следующей строке
        while (end < len && (text[end] == L'\n' || text[end] == L'\r'))
            end++;

        start = end;
    }

    FreePool(file.Message);
    return EFI_SUCCESS;
}
CHAR16* GetFileExtension(IN CHAR16 *path) {
    if (path == NULL)
        return NULL;

    CHAR16 *lastDot = NULL;
    CHAR16 *p = path;

    // Идём по тексту до \0 и ищем последнюю '.'
    while (*p != L'\0') {
        if (*p == L'.')
            lastDot = p;
        p++;
    }

    // Точки нет → расширение отсутствует
    if (lastDot == NULL)
        return NULL;

    // Если точка последняя (строка заканчивается на '.') → нет расширения
    if (*(lastDot + 1) == L'\0')
        return NULL;

    // Возвращаем указатель на начало расширения
    return lastDot + 1;
}
CHAR16 **SplitStringTo2D(CHAR16 *Src, CHAR16 *Delim)
{
    if (Src == NULL || Delim == NULL) return NULL;

    // Считаем количество токенов (для выделения массива указателей)
    UINTN  TokenCount = 0;
    CHAR16 *ptr       = Src;
    while ((ptr = StrToken(&ptr, Delim)) != NULL) {
        ++TokenCount;
    }

    if (TokenCount == 0) return NULL;          // строка пустая или нет токенов

    // Выделяем массив указателей
    CHAR16 **Result = AllocateZeroPool((TokenCount + 1) * sizeof(CHAR16*));
    if (!Result) return NULL;

    // Второй проход – копируем сами токены
    ptr       = Src;
    UINTN idx = 0;
    while ((ptr = StrToken(&ptr, Delim)) != NULL) {
        // Выделяем память под один токен + нуль‑терминатор
        CHAR16 *token = AllocateZeroPool((StrLen(ptr) + 1) * sizeof(CHAR16));
        if (!token) {                      // ошибка – чистим всё и возвращаем NULL
            for (UINTN j = 0; j < idx; ++j)
                FreePool(Result[j]);
            FreePool(Result);
            return NULL;
        }
        StrCpy(token, ptr);                 // копируем токен
        Result[idx++] = token;              // сохраняем в массиве
    }

    Result[TokenCount] = NULL;               // завершающий нулевой указатель
    return Result;
}
bool HandleIndirectCommand(
    CHAR16 *cmd,
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable,
    UINT32 color
) {
    // Базовые каталоги
    CHAR16 *dirBin  = L"bin";
    CHAR16 *dirCmd  = L"bin";

    // Создаём структуру, если нет
    FAT32_CreateDir(dirBin);
    FAT32_CreateDir(dirCmd);

    // Путь к .link
    CHAR16 path[256];
    SPrint(path, sizeof(path), L"%s/%s.link", dirCmd, cmd);

    // Читаем файл .link
    struct EC16 data = FAT32_ReadFile(path);

    if (EFI_ERROR(data.Status) || data.Message == NULL) {
        console_print(L"Команда не найдена\n", color);
        return FALSE;
    }

    // Получаем значение внутри .link (например: "app.pex")
    CHAR16 *target = data.Message;

    // Определяем расширение
    CHAR16 *ext = GetFileExtension(target);

    // ---- Запуск pex ----
    if (StrCmp(ext, L"pex") == 0) {
        struct EES st = start(target, ImageHandle);
        if (EFI_ERROR(st.Status)) {
            console_print(L"Ошибка запуска pex\n", color);
            FreePool(target);
            return FALSE;
        }
        FreePool(target);
        return TRUE;
    }

    // ---- Запуск .bat ----
    if (StrCmp(ext, L"bat") == 0) {
        // Читаем .bat
        struct EC16 bat = FAT32_ReadFile(target);
        if (EFI_ERROR(bat.Status) || bat.Message == NULL) {
            console_print(L"Ошибка чтения .bat\n", color);
            FreePool(target);
            return FALSE;
        }

        // Выполняем построчно
        CHAR16 *ptr = bat.Message;
        CHAR16 *line = ptr;

        while (*ptr != 0) {
            if (*ptr == L'\n') {
                *ptr = 0;
                ExecuteLine(line, ImageHandle, SystemTable, color);
                line = ptr + 1;
            }
            ptr++;
        }

        // последняя строка
        if (StrLen(line) > 0)
            ExecuteLine(line, ImageHandle, SystemTable, color);

        FreePool(bat.Message);
        FreePool(target);
        return TRUE;
    }

    // ---- Запуск txt (как print) ----
    if (StrCmp(ext, L"txt") == 0) {
        struct EC16 txt = FAT32_ReadFile(target);
        if (!EFI_ERROR(txt.Status) && txt.Message)
            console_print(txt.Message, color);

        if (txt.Message) FreePool(txt.Message);
        FreePool(target);
        return TRUE;
    }

    // Неизвестное расширение
    console_print(L"Неизвестный тип файла\n", color);
    FreePool(target);
    return FALSE;
}
EFI_STATUS run_comsole(CHAR16 *line,EFI_HANDLE ImageHandle){
    CHAR16 *args[16];
    UINTN argc = FAT32_SplitLine(line, args, 16);
        if (StrCmp(args[0], L"cd") == 0) {
            if (argc < 2) {
                console_print(L"Usage: cd <dir>\n", color);
            } else {
                EFI_STATUS s = FAT32_ChangeDirEx(args[1]);
                if (EFI_ERROR(s)) Print(L"cd: cannot change dir: %r\n", s);
            }
        }else if (StrCmp(args[0], L"ls") == 0) {
            struct EC16 list = FAT32_ListDir();
            if (EFI_ERROR(list.Status)) {
                Print(L"ls: failed: %r\n", list.Status);
            } else {
                if (list.Message) {
                    console_print(list.Message, color);
                    FreePool(list.Message);
                }
            }
        }else if (StrCmp(args[0], L"start") == 0) {
            CHAR16 *ext = GetFileExtension(args[1]);
            if (ext != NULL && StrCmp(ext, L"pex") == 0) {
                struct EES status = start(args[1],ImageHandle);
                if(EFI_ERROR(status.Status)){
                    CHAR16 buf[128];
                    SPrint(buf, sizeof(buf), L"start app: failed: %r\n", status.Status);
                    console_print(buf, color);
                }
            }else if (ext != NULL && StrCmp(ext, L"bat") == 0) {
                RunScriptFile(args[1]);
            }else if (ext != NULL && StrCmp(ext, L"bat") == 0) {
                RunScriptFile(args[1]);
            }
        }else if(StrCmp(args[0], L"print") == 0){
            // Подсчёт длины результата
            UINTN totalLen = 0;
            for (UINTN i = 1; i < argc; i++)
                totalLen += StrLen(args[i]) + 1; // пробел или конец

            CHAR16 *text = AllocateZeroPool((totalLen + 1) * sizeof(CHAR16));
            if (!text) {
                console_print(L"print: no mem\n", color);
                return EFI_SUCCESS;
            }

            // Склеиваем текст
            for (UINTN i = 1; i < argc; i++) {
                StrCat(text, args[i]);
                if (i + 1 < argc) StrCat(text, L" ");
            }

            for (UINTN i = 0; text[i] != 0; i++) {

                // Проверяем на "\n"
                if (text[i] == L'\\' && text[i+1] == L'n') {

                    // Если это НЕ экранированная последовательность ("\\n")
                    if (i == 0 || text[i-1] != L'\\') {

                        text[i] = L'\n';    // заменяем на перенос
                        // сдвигаем строку, удаляя 'n'
                        for (UINTN j = i + 1; text[j] != 0; j++)
                            text[j] = text[j + 1];

                    } else {
                        // Экранированная конструкция "\\n" → превращаем в "\n"
                        // то есть удаляем один лишний '\'
                        for (UINTN j = i; text[j] != 0; j++)
                            text[j - 1] = text[j];

                        i--; // продолжаем с корректной позиции
                    }
                }
            }
            console_print(text, color);
        }else if(StrCmp(args[0], L"info-OS") == 0){
            console_print(L"OS: POSK (ParrotOS Kernel) a0.4\n",color);
            console_print(L"Version Console: a0.4\n",color);
            console_print(L"Build: 15\nProduct Author: ParrotSoft\n",color);
        }else if (StrCmp(args[0], L"bmp") == 0) {
            console_clear_screen(Black);
            struct EC16 file;
            EFI_STATUS fileStatus = FAT32_ReadFileByPath(args[1], &file);

            if (EFI_ERROR(fileStatus)) {
                console_print(L"bmp: failed to load file\n", color);
                return EFI_SUCCESS;
            }

            if (file.Message == NULL) {
                console_print(L"bmp: empty file\n", color);
                return EFI_SUCCESS;
            }

            // ----- Читаем размеры BMP -----
            UINT8 *bmp = (UINT8*)file.Message;

            // Проверяем сигнатуру 'BM'
            if (bmp[0] != 'B' || bmp[1] != 'M') {
                console_print(L"bmp: invalid BMP file\n", color);
                return EFI_SUCCESS;
            }

            // Из BITMAPINFOHEADER
            UINT32 width  = *(UINT32*)(bmp + 18);
            INT32  height = *(INT32*)(bmp + 22);

            // Высота может быть отрицательной — переворачивать не нужно
            if (height < 0) height = -height;

            // ---- Центровка ----
            INT32 bmp_x = (vmode.width  / 2) - (width  / 2);
            INT32 bmp_y = (vmode.height / 2) - (height / 2);

            // ---- Рисуем ----
            EFI_STATUS st = draw_bmp_from_memory_safe(bmp, file.FileSize, bmp_x, bmp_y);

            if (EFI_ERROR(st)) {
                console_print(L"bmp: draw failed\n", color);
            }

            FreePool(file.Message);
        }
        else if (StrCmp(args[0], L"rid") == 0) {
            Fat32_RegisterrsDisk();
        }
        else if (StrCmp(args[0], L"pwd") == 0) {
            CHAR16 *p = FAT32_PrintCurrentPath();
            if (p) console_print(p, color);
            console_print(L"\n", color);
        }else if (StrCmp(args[0], L"cat") == 0) {
            if (argc < 2) {
                console_print(L"Usage: cat <file>\n", color);
            } else {
                struct EC16 s = FAT32_ReadFile(args[1]);
                if (EFI_ERROR(s.Status)) {
                    CHAR16 buf[128];
                    SPrint(buf, sizeof(buf), L"cat: failed: %r\n", s.Status);
                    console_print(buf, color);
                } else {
                    if (s.Message) {
                        console_print(s.Message, color);
                        FreePool(s.Message);
                    }
                }
            }
        }else if (StrCmp(args[0], L"shutdown") == 0) {
            shutdown(ST);
        }else if(StrCmp(args[0], L"reboot") == 0){
            reboot(ST);
        }else if (StrCmp(args[0], L"wf") == 0) {
            if (argc < 3) {
                console_print(L"Usage: wf <file> <text>\n", color);
            } else {
                // args[2] — текст, StrLen(args[2]) символов
                UINTN totalLen = 0;
                for (UINTN i = 2; i < argc; i++)
                    totalLen += StrLen(args[i]) + 1; // +1 за пробел
        
                CHAR16 *text = AllocateZeroPool((totalLen + 1) * sizeof(CHAR16));
                if (!text) {
                    console_print(L"wf: no mem\n", color);
                    return EFI_SUCCESS;
                }
                for (UINTN i = 2; i < argc; i++) {
                    StrCat(text, args[i]);
                    if (i + 1 < argc) StrCat(text, L" ");
                }
                for (UINTN i = 0; text[i] != 0; i++) {
                    // Проверяем на "\n"
                    if (text[i] == L'\\' && text[i+1] == L'n') {
                        if (i == 0 || text[i-1] != L'\\') {
                            text[i] = L'\n';    // заменяем на перенос
                            for (UINTN j = i + 1; text[j] != 0; j++)
                                text[j] = text[j + 1];

                        } else {
                            for (UINTN j = i; text[j] != 0; j++)
                                text[j - 1] = text[j];

                            i--; // продолжаем с корректной позиции
                        }
                    }
                }
                EFI_STATUS s = FAT32_WriteFile(args[1], (UINT16*)text, StrLen(text));
                if (EFI_ERROR(s)) {
                    CHAR16 buf[128];
                    SPrint(buf, sizeof(buf), L"wf: failed: %r\n", s);
                    console_print(buf, color);
                }
            }
        }else if (StrCmp(args[0], L"cf") == 0) {
            if (argc < 2) {
                console_print(L"Usage: cf <file>\n", color);
            } else {
                EFI_STATUS s = FAT32_CreateFile(args[1]);
                if (EFI_ERROR(s)) Print(L"cf: failed: %r\n", s);
            }
        }else if (StrCmp(args[0], L"mkdir") == 0) {
            if (argc < 2) {
                console_print(L"Usage: mkdir <name>\n", color);
            } else {
                EFI_STATUS s = FAT32_CreateDir(args[1]);
                if (EFI_ERROR(s)) Print(L"mkdir: failed: %r\n", s);
            }
        }else if (StrCmp(args[0], L"rm") == 0) {
            if (argc < 2) {
                console_print(L"Usage: rm <file|dir>\n", color);
            } else {
                EFI_STATUS s = FAT32_DeleteEntry(args[1]); // внутри должна проверяться папка/файл
                if (EFI_ERROR(s)) Print(L"rm: failed: %r\n", s);
            }
        }else if (StrCmp(args[0], L"cp") == 0) {
            if (argc < 3) {
                console_print(L"Usage: cp <src> <dest>\n", color);
            } else {
                EFI_STATUS s = FAT32_CopyFile(args[1], args[2]);
                if (EFI_ERROR(s)) Print(L"cp: failed: %r\n", s);
            }
        }else if (StrCmp(args[0], L"mv") == 0) {
            if (argc < 3) {
                console_print(L"Usage: mv <src> <dest>\n", color);
            } else {
                EFI_STATUS s = FAT32_MoveFile(args[1], args[2]);
                if (EFI_ERROR(s)) Print(L"mv: failed: %r\n", s);
            }
        }else if (StrCmp(args[0], L"disklist") == 0) {
            console_print(L"Available disks:\n", color);
            struct EC16 disks = FAT32_ListDisks();
            if (EFI_ERROR(disks.Status)) {
                Print(L"disklist: failed: %r\n", disks.Status);
            } else {
                if (disks.Message) {
                    console_print(disks.Message, color);
                    FreePool(disks.Message);
                }
            }
        }else if (StrCmp(args[0], L"help") == 0) {
            console_print(L"Commands:\n", color);
            console_print(L"  cd <dir>          - change directory\n", color);
            console_print(L"  ls                - list files\n", color);
            console_print(L"  pwd               - print current directory\n", color);
            console_print(L"  cat <file>        - print file content\n", color);
            console_print(L"  wf <file> <text>  - write text to file (overwrite)\n", color);
            console_print(L"  cf <file>         - create empty file\n", color);
            console_print(L"  mkdir <name>      - create directory\n", color);
            console_print(L"  rm <file|dir>     - remove file or directory\n", color);
            console_print(L"  cp <src> <dest>   - copy file\n", color);
            console_print(L"  mv <src> <dest>   - move/rename file\n", color);
            console_print(L"  disklist          - list registered disks\n", color);
            console_print(L"  clear | cls       - clear screen\n", color);
            console_print(L"  exit              - exit shell\n", color);
            console_print(L"  rid               - reinit disks\n", color);
            console_print(L"  start <file>      - run app file\n", color);
            console_print(L"  print <text       - Draw Text\n", color);
            console_print(L"  bmp <file>        - Draw Bitmap\n", color);
            console_print(L"  info-OS           - get info OS\n", color);
            console_print(L"  reboot            - ReBoot System\n", color);
            console_print(L"  shutdown          - Shutdown System\n", color);
        }else if (StrCmp(args[0], L"clear") == 0 || StrCmp(args[0], L"clr") == 0) {
            console_clear_screen(Black);
        }else {
            //if(!HandleIndirectCommand(args[0],ImageHandle,SystemTable,color)){
                console_print(args[0], color);
                console_print(L": unknown command\n", color);
            //}
        }
}

EFI_STATUS console_main(EFI_SYSTEM_TABLE *SystemTable,EFI_HANDLE ImageHandle, bool enable_cursor) {
    console_init(SystemTable, enable_cursor);
    Fat32_RegisterrsDisk();
    console_print(L"ParrotOS Shell\n", Green);

    while (1) {
        // Текущий путь
        CHAR16 *path = FAT32_PrintCurrentPath();
        if (path) console_print(path, Green-110);
        console_print(L" :> ", Green);

        CHAR16 *line = console_ReadLine();
        if (!line) continue;
        if (StrCmp(line, L"exit") == 0) {
         break;
        }
        run_comsole(line,ImageHandle);
    }

    return EFI_SUCCESS;
}
