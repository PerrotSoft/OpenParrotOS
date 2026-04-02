#!/bin/bash


BUILD_DIR=build


mkdir -p $BUILD_DIR

nasm -f elf src/boot.asm -o $BUILD_DIR/bootloader.o
gcc -m32 -ffreestanding -fno-pic -c src/kernel/kernel.c -o $BUILD_DIR/kernel.o

gcc -m32 -ffreestanding -fno-pic -c src/kernel/include/c/tasks.c -o $BUILD_DIR/tasks.o
gcc -m32 -ffreestanding -fno-pic -c src/kernel/include/c/ramfs.c -o $BUILD_DIR/ramfs.o

ld -m elf_i386 -T linker.ld -o $BUILD_DIR/kernel.elf \
    $BUILD_DIR/bootloader.o $BUILD_DIR/kernel.o $BUILD_DIR/tasks.o $BUILD_DIR/ramfs.o
objcopy -O binary $BUILD_DIR/kernel.elf $BUILD_DIR/bootloader.bin

genisoimage -R -b bootloader.bin -no-emul-boot -boot-load-size 100 -o os.iso $BUILD_DIR/

prm -rf $BUILD_DIR

qemu-system-x86_64 -cdrom os.iso
