#include "../sys.h"
#include "../drivers/fat32.h"
uint64_t ID_counter = 1;
struct EES start(CHAR16 *name, EFI_HANDLE ImageHandle) {
    struct EES status;
    struct EC16 file = FAT32_ReadFile(name);
    if (EFI_ERROR(file.Status)) {
        status.Status = file.Status;
        return status;
    }
    CHAR16 *msg16 = file.Message;
    UINT8  *raw   = (UINT8*)file.Message;
    // Проверка MZ
    if (raw[0] != 0x4D || raw[1] != 0x5A) {
        FreePool(file.Message);
        status.Status = EFI_NOT_FOUND;
        return status;
    }
    status.ID = ID_counter++;
    UINT8 version_protocol = raw[2];
    if (version_protocol == 0) {
        status.Start_Bite = 3;
        StrCpy(status.NAME, L"EFI APP");

        status.data = raw + 3;
        UINTN bin_size = file.FileSize - 3;

        status.Status = start_from_binary(status.data, bin_size, ImageHandle);
    }
    else if (version_protocol == 1) {
        status.Start_Bite = raw[3];
        CHAR16 *name_ptr = (CHAR16*)(raw + 4);
        UINTN process_name_len = StrLen(name_ptr);
        for (UINTN i = 0; i <= process_name_len; i++) {
            status.NAME[i] = name_ptr[i];
        }
        UINTN header_size = 4 + (process_name_len + 1) * sizeof(CHAR16);
        status.data = raw + header_size;
        UINTN bin_size = file.FileSize - header_size;
        status.Status = start_from_binary(status.data, bin_size, ImageHandle);
    }
    else {
        status.Status = 1001;
        Print(L"Unknown protocol version: %d\n", version_protocol);
        FreePool(file.Message);
        return status;
    }

    FreePool(file.Message);
    return status;
}
EFI_STATUS start_from_binary(VOID *Buffer,UINTN BufferSize,EFI_HANDLE ParentImageHandle){
    EFI_STATUS Status;
    EFI_HANDLE ImageHandle = NULL;
    // Загружаем EFI-изображение из памяти
    Status = uefi_call_wrapper(
        BS->LoadImage,
        6,
        FALSE,                 // BootPolicy = FALSE
        ParentImageHandle,    // Родительское изображение
        NULL,                 // Нет DevicePath, мы грузим из памяти
        Buffer,               // Сам бинарный код
        BufferSize,           // Его размер
        &ImageHandle          // Получаем handle
    );

    if (EFI_ERROR(Status)) {
        Print(L"LoadImage from memory failed: %r\n", Status);
        return Status;
    }
    // Стартуем загруженный EFI-модуль
    Status = uefi_call_wrapper(
        BS->StartImage,
        3,
        ImageHandle,
        NULL,
        NULL
    );
    return Status;
}
static inline void reboot(EFI_SYSTEM_TABLE *SystemTable) {
    SystemTable->RuntimeServices->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
    // если не reboot'нуло – fallback
    SystemTable->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
    while (1);
}

static inline void shutdown(EFI_SYSTEM_TABLE *SystemTable) {
    // Используйте SystemTable->RuntimeServices->ResetSystem 
    // если вы не хотите полагаться на InitializeLib() и gRT из efilib.h,
    // но в gnu-efi проще использовать gRT.

    // Вызов функции ResetSystem через глобальный указатель gRT
    gRT->ResetSystem(
        EfiResetCold, // Попытка полной аппаратной перезагрузки
        EFI_SUCCESS,
        0,
        NULL
    );

    // Этот цикл гарантирует, что программа не завершится некорректно, 
    // если прошивка не смогла немедленно выключить систему.
    while (1); 
}