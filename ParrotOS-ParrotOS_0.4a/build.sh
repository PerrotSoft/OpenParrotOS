make
BUILD_FILE="build_number.txt"  # файл с номером билда
# Сделаем пустой файл-образ на 64 МБ
dd if=/dev/zero of=boot.img bs=1M count=1

# Отформатируем как FAT32
mkfs.vfat boot.img

# Смонтируем и скопируем EFI-файлы
mkdir mnt
sudo mount -o loop boot.img mnt
sudo mkdir -p mnt/EFI/BOOT
sudo cp boot.efi mnt/EFI/BOOT/BOOTX64.EFI
sudo cp ico_100x100.bmp mnt/

sudo umount mnt
mkdir -p iso/EFI/BOOT
cp boot.efi iso/EFI/BOOT/BOOTX64.EFI
make clean
# Создать ISO с загрузочным каталогом EFI
mkisofs -U -A "MyUEFI" -J -joliet-long -r -v -T \
  -eltorito-alt-boot \
  -e EFI/BOOT/BOOTX64.EFI \
  -no-emul-boot -o boot.iso iso
qemu-system-x86_64 -hda boot.img -m 128M -bios OVMF.fd


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
