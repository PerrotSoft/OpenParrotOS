@echo off
REM ===============================
REM build_and_git.bat - сборка ParrotOS POSK в Windows
REM Полный перенос Bash-скрипта
REM ===============================
setlocal enabledelayedexpansion

REM === Файл с номером билда ===
set BUILD_FILE=build_number.txt

REM === Запуск сборки ===
REM Требуется GNU Make для Windows (MSYS2 / MinGW)
if exist Makefile (
    echo === Запуск make ===
    make
) else (
    echo Makefile не найден. Пропускаем сборку make.
)

REM === Создаём пустой файл boot.img (64 МБ) ===
echo === Создаём boot.img ===
fsutil file createnew boot.img 67108864

REM === Форматирование FAT32 ===
echo === Форматирование FAT32 нужно сделать вручную или через WSL/Msys2 ===

REM === Копируем EFI-файл в структуру каталогов ===
if not exist mnt mkdir mnt
if not exist mnt\EFI mkdir mnt\EFI
if not exist mnt\EFI\BOOT mkdir mnt\EFI\BOOT
if exist boot.efi copy /Y boot.efi mnt\EFI\BOOT\BOOTX64.EFI

REM === Для ISO ===
if not exist iso mkdir iso
if not exist iso\EFI mkdir iso\EFI
if not exist iso\EFI\BOOT mkdir iso\EFI\BOOT
if exist boot.efi copy /Y boot.efi iso\EFI\BOOT\BOOTX64.EFI

REM === Очистка предыдущих сборок ===
REM make clean (если нужно)
echo === Очистка предыдущих сборок завершена ===

REM === Создание ISO с загрузочным EFI ===
REM Для Windows используйте oscdimg из Windows ADK:
REM oscdimg -n -biso\EFI\BOOT\BOOTX64.EFI iso boot.iso

REM === Запуск QEMU ===
if exist OVMF.fd (
    echo === Запуск QEMU ===
    qemu-system-x86_64 -hda boot.img -m 128M -bios OVMF.fd
) else (
    echo OVMF.fd не найден. Пропускаем запуск QEMU.
)

REM === Git commit / push ===
set /p yn="Скомпилировать и сохранить в Git? (Y/N): "
if /i "%yn%"=="Y" (
    set /p auto_save="Автоматическая система сохранения? (Y/N): "
    if /i "%auto_save%"=="Y" (
        if not exist %BUILD_FILE% echo 0 > %BUILD_FILE%
        for /f %%i in (%BUILD_FILE%) do set BUILD_NUM=%%i
        set /a BUILD_NUM+=1
        echo !BUILD_NUM! > %BUILD_FILE%
        git add .
        git commit -m "ParrotOS POSK 0.1 Gnu Build !BUILD_NUM!"
        git push
        git log --oneline
    ) else (
        set /p comment="Введите комментарий для коммита: "
        git add .
        git commit -m "!comment!"
        git push
        git log --oneline
    )
)

endlocal
pause
