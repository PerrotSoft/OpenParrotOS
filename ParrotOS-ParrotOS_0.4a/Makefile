ARCH    := x86_64
QEMU    := qemu-system-$(ARCH)
BUILD   := build
OVMF_DIR:= ovmf

# Пути до системных OVMF
OVMF_FW_SRC   := /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS_SRC := /usr/share/OVMF/OVMF_VARS_4M.fd

# Локальные копии для QEMU
OVMF_FW   := $(OVMF_DIR)/OVMF_CODE.fd
OVMF_VARS := $(OVMF_DIR)/OVMF_VARS.fd

EFIINC   := gnu-efi/inc
EFILIB   := gnu-efi/$(ARCH)/lib
GNUEFI   := gnu-efi/$(ARCH)/gnuefi
LDSCRIPT := gnu-efi/gnuefi/elf_$(ARCH)_efi.lds

CFLAGS   := -I $(EFIINC) -I include -fpic -ffreestanding -fno-stack-protector \
            -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args \
            -Wall -Wextra
LDFLAGS  := -shared -Bsymbolic -L $(EFILIB) -L $(GNUEFI) -T $(LDSCRIPT)

# Все исходники
SRC := boot.c $(wildcard include/c/*.c)
OBJ := $(SRC:.c=.o)

.PHONY: all build run clean rebuild

all: build

# Полный билд: собираем boot.efi + кладём в FAT
build: boot.efi $(OVMF_FW) $(OVMF_VARS)
	mkdir -p $(BUILD)/EFI/BOOT
	cp boot.efi $(BUILD)/EFI/BOOT/BootX64.efi
	echo "\\EFI\\BOOT\\BOOTX64.EFI" > $(BUILD)/startup.nsh
	@echo "Build completed. EFI files are in: $(BUILD)"

# Общие правила для компиляции .c → .o
%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

# Линковка
boot.so: $(OBJ)
	ld $(LDFLAGS) $(GNUEFI)/crt0-efi-$(ARCH).o $(OBJ) \
	   -o boot.so -lgnuefi -lefi

# Конвертация в EFI
boot.efi: boot.so
	objcopy -O efi-app-$(ARCH) \
	        -j .text -j .sdata -j .data -j .rodata \
	        -j .dynamic -j .dynsym -j .rel* -j .rela* -j .reloc \
	        boot.so boot.efi

# Копируем OVMF, если локально нет
$(OVMF_FW):
	mkdir -p $(OVMF_DIR)
	cp $(OVMF_FW_SRC) $(OVMF_FW)

$(OVMF_VARS):
	mkdir -p $(OVMF_DIR)
	cp $(OVMF_VARS_SRC) $(OVMF_VARS)

# Запуск QEMU
run: build
	$(QEMU) -nodefaults -vga std \
		-machine q35,accel=kvm:tcg \
		-m 128M \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_FW) \
		-drive if=pflash,format=raw,file=$(OVMF_VARS) \
		-drive format=raw,file=fat:rw:$(BUILD) \
		-serial stdio \
		-monitor stdio

clean:
	rm -rf $(OVMF_DIR) $(OBJ) *.so *.efi *.o

rebuild: clean all
