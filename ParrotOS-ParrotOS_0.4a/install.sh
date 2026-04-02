#!/bin/bash

# ==============================================================================
# Скрипт для установки инструментов на Debian/Ubuntu-подобных системах.
# ==============================================================================

# Проверка, что скрипт запущен с правами root
if [ "$EUID" -ne 0 ]; then
  echo "Пожалуйста, запустите этот скрипт с sudo: sudo bash $0"
  exit 1
fi

# ============================ БЛОК 1: ОСНОВНАЯ УСТАНОВКА ИНСТРУМЕНТОВ ============================

echo "--- 1. Обновление списка пакетов ---"
apt update -y

echo "--- 2. Установка базовых инструментов, Qt5/Qt6 и QEMU ---"
# Объединяем все пакеты из ваших логов в одну команду
apt install -y \
    make build-essential git nasm binutils \
    qemu-system-x86 qemu-system genisoimage xorriso \
    gcc-multilib g++-multilib \
    dos2unix dosfstools libssl-dev \
    cmake gdb ninja-build \
    software-properties-common wget curl \
    qtbase5-dev qtchooser qt5-qmake qt5-default qtbase5-dev-tools \
    qt4-dev-tools libqt4-dev \
    qt6-base-dev qt6-tools-dev-tools qt6-l10n-tools

# ============================ БЛОК 2: НАСТРОЙКА GCC/G++ ============================

echo "--- 3. Настройка и установка GCC/G++ 10-13 через PPA ---"
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt update

# Установка всех необходимых версий GCC/G++
apt install -y gcc-10 g++-10 gcc-11 g++-11 gcc-12 g++-12 gcc-13 g++-13

echo "--- 4. Настройка update-alternatives для переключения версий ---"
for ver in 10 11 12 13; do
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-$ver $ver
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-$ver $ver
done
echo "Настройка update-alternatives завершена. Активные версии: $(gcc --version | head -n 1)"

# ============================ БЛОК 3: УСТАНОВКА CMake 4.0.0 ============================

CMAKE_VER="4.0.0" # ⚠️ ОСТОРОЖНО: ЭТА ВЕРСИЯ ВЕРОЯТНО ВЫЗОВЕТ ОШИБКУ 404
CMAKE_DIR="/opt/cmake-$CMAKE_VER"
CMAKE_FILE="cmake-$CMAKE_VER-linux-x86_64.sh"
CMAKE_URL="https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/$CMAKE_FILE"

echo "--- 5. Попытка скачивания и установки CMake $CMAKE_VER ---"
wget -O /tmp/$CMAKE_FILE $CMAKE_URL 2>/dev/null
export PATH=/opt/cmake-4/cmake-4.0.0-linux-x86_64/bin:$PATH
if [ $? -eq 0 ]; then
    mkdir -p "$CMAKE_DIR"
    sh /tmp/$CMAKE_FILE --prefix="$CMAKE_DIR" --skip-license
    rm /tmp/$CMAKE_FILE
    
    echo "--- Настройка PATH для CMake $CMAKE_VER ---"
    # Добавляем путь в ~/.bashrc для постоянного использования (для SUDO_USER)
    echo "export PATH=\"$CMAKE_DIR/bin:\$PATH\"" >> /home/$SUDO_USER/.bashrc
    
    # Применяем изменения к текущей сессии
    export PATH="$CMAKE_DIR/bin:$PATH"
    
    echo "✅ CMake $CMAKE_VER успешно установлен и добавлен в PATH."
else
    echo "❌ ОШИБКА: Не удалось скачать CMake $CMAKE_VER. Используется версия из APT."
fi

# ============================ БЛОК 4: ПОЛНАЯ ПРОВЕРКА УСТАНОВКИ ============================

echo ""
echo "========================================================"
echo "          ✅ ПРОВЕРКА УСТАНОВЛЕННЫХ ИНСТРУМЕНТОВ         "
echo "========================================================"

function check_command() {
    local cmd=$1
    local version_flag=$2

    if command -v "$cmd" >/dev/null 2>&1; then
        output=$(eval "$cmd $version_flag" 2>&1 | head -n 1)
        echo "✅ $cmd: OK ($output)"
    else
        echo "❌ $cmd: НЕ НАЙДЕНО"
    fi
}

echo -e "\n--- Инструменты сборки и QEMU ---"
check_command make --version
check_command git --version
check_command nasm -v
check_command qemu-system-x86_64 --version
check_command ninja --version

echo -e "\n--- Активные компиляторы ---"
check_command gcc --version
check_command g++ --version

echo -e "\n--- Установленные версии GCC/G++ ---"
# Проверка наличия пакетов gcc/g++ 10-13
dpkg -l | grep -E '^(ii|rc).*(gcc|g\+\+)-(10|11|12|13)'

echo -e "\n--- CMake ---"
check_command cmake --version

echo -e "\n--- Qt Framework ---"
check_command qmake -v # Проверка Qt5
check_command qmake6 -v # Проверка Qt6

echo -e "\n========================================================"
echo "          ✅ Скрипт установки и проверки завершен.       "
echo "========================================================"
cmake --version
# Сообщаем пользователю о необходимости применения PATH
if [ -d "$CMAKE_DIR" ]; then
    echo "!!! Для использования CMake $CMAKE_VER выполните: source ~/.bashrc"
fi