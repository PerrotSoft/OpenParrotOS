BITS 16
global _start
extern kernel_main

%define VBE_BUF_SEG  0x9000         ; буфер для VBE ModeInfoBlock в низкой памяти
%define VBE_BUF_OFF  0x0000
%define VBE_BUF_PHYS ((VBE_BUF_SEG << 4) + VBE_BUF_OFF)  ; 0x00090000

section .text

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; =========================
    ; Получаем VBE ModeInfoBlock в 0x9000:0000
    ; =========================
    mov ax, 0x4F01          ; VBE: получить информацию о режиме
    mov cx, 0x118           ; 800x600x32
    mov ax, VBE_BUF_SEG
    mov es, ax
    mov di, VBE_BUF_OFF
    mov ax, 0x4F01
    int 0x10

    ; =========================
    ; Установим VBE графический режим (LFB)
    ; =========================
    mov ax, 0x4F02
    mov bx, 0x118
    or  bx, 0x4000          ; линейный фреймбуфер
    int 0x10

    ; =========================
    ; Настройка GDT и переход в Protected Mode
    ; =========================
    lgdt [gdt_pointer]
    mov eax, cr0
    or  eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:init_pm

gdt_start:
    dq 0x0
gdt_code:
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
gdt_data:
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_pointer:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

BITS 32
init_pm:
    cli
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x10000

    ; Читаем ModeInfo из физического 0x00090000 (paging не включен → identity)
    mov eax, dword [VBE_BUF_PHYS + 0x28]   ; PhysBasePtr (LFB)
    movzx ebx, word  [VBE_BUF_PHYS + 0x12] ; XResolution
    movzx ecx, word  [VBE_BUF_PHYS + 0x14] ; YResolution
    movzx edx, byte  [VBE_BUF_PHYS + 0x19] ; BitsPerPixel
    movzx esi, word  [VBE_BUF_PHYS + 0x32] ; LinBytesPerScanLine (может быть 0)
    test   esi, esi
    jnz    .have_pitch
    movzx  esi, word  [VBE_BUF_PHYS + 0x10] ; BytesPerScanLine
.have_pitch:
    push esi    ; pitch
    push edx    ; bpp
    push ecx    ; height
    push ebx    ; width
    push eax    ; physbase (LFB phys addr)
    call kernel_main

.hang:
    hlt
    jmp .hang
