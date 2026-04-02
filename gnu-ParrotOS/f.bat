cd qemu
qemu-system-x86_64 -cdrom os.iso -boot d -vga std -drive file=../hdd.img,format=qcow2,index=0,media=disk
