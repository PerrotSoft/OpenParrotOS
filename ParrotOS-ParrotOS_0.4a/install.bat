@echo off
REM setup-windows-tools.bat
REM Аналог команд для WSL, но для Windows

echo === 1. Обновление системы (Windows Update) ===
echo Обновление Windows выполняется через встроенные средства, здесь пропускаем.

echo === 2. Установка необходимых инструментов ===
echo Установка Git
winget install --id Git.Git -e --source winget

echo Установка NASM
winget install --id NASM.NASM -e --source winget

echo Установка QEMU
winget install --id FOSSHub.QEMU -e --source winget

echo Установка Binutils
echo В Windows binutils можно использовать через MSYS2 или WSL. Пропускаем для нативного cmd.

echo Установка Genisoimage / Xorriso
echo Аналоги для Windows: cdrtools или xorriso через MSYS2. Пропускаем для нативного cmd.

echo === 3. Установка 32-битной поддержки ===
echo В Windows это через Visual Studio и MSVC v142 toolset. Нет прямого аналога sudo dpkg --add-architecture i386.

echo === 4. Установка multilib компиляторов ===
echo В Windows через MSVC можно выбрать x86/x64 при установке toolset.

echo === 5. Установка VS Code ===
echo Установка через winget
winget install --id Microsoft.VisualStudioCode -e --source winget

echo === Проверка установленных инструментов ===
git --version
nasm -v
qemu-system-x86_64 --version
code --version

echo === Установка завершена ===
REM setup-dev-windows.bat
REM Установка и настройка среды разработки C++ для VS/CLion с Qt, CMake, Ninja

echo === 1. Проверяем наличие Visual Studio Installer ===
where vswhere >nul 2>&1
IF ERRORLEVEL 1 (
    echo Пожалуйста, установите Visual Studio 2022 или 2023 с "Desktop development with C++"
    pause
    exit /b
)

echo === 2. Установка MSVC toolsets через vswhere/installer ===
REM Пример для проверки/установки v143 и v142
"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe" modify ^
    --installPath "C:\Program Files\Microsoft Visual Studio\2022\Community" ^
    --add Microsoft.VisualStudio.Workload.NativeDesktop ^
    --includeRecommended ^
    --quiet

echo === 3. Скачиваем CMake 4.x ===
set CMAKE_VER=4.0.0
set CMAKE_DIR=C:\Tools\CMake4
mkdir "%CMAKE_DIR%"
powershell -Command "Invoke-WebRequest -Uri https://github.com/Kitware/CMake/releases/download/v%CMKAE_VER%/cmake-%CMAKE_VER%-windows-x86_64.msi -OutFile C:\Temp\cmake.msi"
msiexec /i C:\Temp\cmake.msi /quiet /qn /norestart TARGETDIR="%CMAKE_DIR%"
del C:\Temp\cmake.msi

echo === 4. Скачиваем и устанавливаем Ninja ===
set NINJA_DIR=C:\Tools\Ninja
mkdir "%NINJA_DIR%"
powershell -Command "Invoke-WebRequest -Uri https://github.com/ninja-build/ninja/releases/download/v1.11.1/ninja-win.zip -OutFile C:\Temp\ninja.zip"
powershell -Command "Expand-Archive C:\Temp\ninja.zip -DestinationPath %NINJA_DIR%"
del C:\Temp\ninja.zip

echo === 5. Скачиваем Qt Installer для Qt5 и Qt6 ===
REM Qt Online Installer
set QT_INSTALLER=C:\Temp\qt-online-installer.exe
powershell -Command "Invoke-WebRequest -Uri https://download.qt.io/official_releases/online_installers/qt-unified-windows-x86-online.exe -OutFile %QT_INSTALLER%"
echo Запустите установщик %QT_INSTALLER% и выберите Qt5 и Qt6 версии, нужные toolset для MSVC
pause

echo === 6. Настраиваем PATH ===
setx PATH "%CMAKE_DIR%\bin;%NINJA_DIR%;%PATH%"

echo === 7. Проверяем установки ===
cmake --version
ninja --version
cl
echo === Установка завершена! ===
pause
