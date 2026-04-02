#!/bin/bash


BUILD_DIR=build
BUILD_FILE="build_number.txt"  # файл с номером билда

mkdir -p $BUILD_DIR
nasm -f elf src/boot.asm -o $BUILD_DIR/bootloader.o
gcc -m32 -ffreestanding -fno-pic -c src/kernel/kernel.c -o $BUILD_DIR/kernel.o
gcc -m32 -ffreestanding -fno-pic -c src/kernel/include/c/video_driver.c -o $BUILD_DIR/video_driver.o
gcc -m32 -ffreestanding -fno-pic -c src/kernel/include/c/io.c -o $BUILD_DIR/io.o
/gcc -m32 -ffreestanding -fno-pic -c src/kernel/include/c/audio_driver.c -o $BUILD_DIR/audio_driver.o


ld -m elf_i386 -T linker.ld -o $BUILD_DIR/kernel.elf \
    $BUILD_DIR/bootloader.o $BUILD_DIR/kernel.o $BUILD_DIR/io.o $BUILD_DIR/video_driver.o 
objcopy -O binary $BUILD_DIR/kernel.elf $BUILD_DIR/bootloader.bin
rm $BUILD_DIR/bootloader.o $BUILD_DIR/kernel.o $BUILD_DIR/io.o $BUILD_DIR/video_driver.o $BUILD_DIR/kernel.elf
genisoimage -R -b bootloader.bin -no-emul-boot -boot-load-size 200 -o os.iso $BUILD_DIR/

mkdir -p $BUILD_DIR
qemu-system-x86_64 -cdrom os.iso -boot d -vga std \
  -drive file=hdd.img,format=qcow2,index=0,media=disk

read -p "Скомпилировать и сохранить в Git? (Y/N): " yn
case $yn in
    [Yy]* )
        echo "Компиляция завершена, сохранение в Git не требуется."
        read -p "Автоматическая система сохранения? (Y/N): " auto_save
        case $auto_save in
            [Yy]* )
                if [ ! -f $BUILD_FILE ]; then echo 0 > $BUILD_FILE; fi
                BUILD_NUM=$(cat $BUILD_FILE)
                BUILD_NUM=$((BUILD_NUM + 1))
                echo $BUILD_NUM > $BUILD_FILE
                git add .
                git commit -m "ParrotOS POSK 0.1 Gnu Build $BUILD_NUM"
                git push
                git log --oneline

                ;;
            [Nn]* )
                read -p "Введите комментарий для коммита: " comment
                git add .
                git commit -m "$comment"
                git push
                git log --oneline

                ;;
        esac
        ;;
    [Nn]* )
        
esac
