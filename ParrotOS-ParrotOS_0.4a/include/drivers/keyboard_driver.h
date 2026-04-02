#ifndef KEYBOARD_DRIVER_H
#define KEYBOARD_DRIVER_H

#include <efi.h>
#include <efilib.h>

// Возвращает нажатый символ (печатаемый) или 0 для специальных клавиш
static CHAR16 ReadChar(EFI_SYSTEM_TABLE *SystemTable) {
    EFI_INPUT_KEY Key;
    EFI_STATUS Status;

    // Ждем нажатия клавиши
    do {
        Status = uefi_call_wrapper(SystemTable->ConIn->ReadKeyStroke, 2, SystemTable->ConIn, &Key);
        if (Status == EFI_NOT_READY) {
            uefi_call_wrapper(SystemTable->BootServices->WaitForEvent, 3, 1, &SystemTable->ConIn->WaitForKey, NULL);
        }
    } while (Status == EFI_NOT_READY);

    if (!EFI_ERROR(Status)) {
        return Key.UnicodeChar;  // 0 если специальная клавиша
    }

    return 0;
}

#endif // KEYBOARD_DRIVER_H
