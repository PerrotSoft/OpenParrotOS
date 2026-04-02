#ifndef SYSTEM_CONTROL_H
#define SYSTEM_CONTROL_H

#include <efi.h>
#include <efilib.h>
struct EES{
    EFI_STATUS Status;
    CHAR16 NAME[100];
    uint8_t* data;
    uint64_t ID;
    uint64_t Start_Bite;
};
extern uint64_t ID_counter;
static inline void reboot(EFI_SYSTEM_TABLE *SystemTable);
static inline void shutdown(EFI_SYSTEM_TABLE *SystemTable);
EFI_STATUS start_from_binary(VOID *Buffer,UINTN BufferSize,EFI_HANDLE ParentImageHandle);
struct EES start(CHAR16 *name, EFI_HANDLE ImageHandle);
static inline void halt(void) {
    while (1) {
       // __asm__ __volatile__("hlt"); // для x86
    }
}

#endif
