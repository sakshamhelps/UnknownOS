#include <efi.h>
#include <efilib.h>

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"=====================================================\n");
    Print(L"           UNKNOWN OS BOOTLOADER v1.0 (UEFI)         \n");
    Print(L"           India's Independent Operating System      \n");
    Print(L"           Support: byrex.me@proton.me               \n");
    Print(L"=====================================================\n\n");

    Print(L"[*] Initializing Graphics Output Protocol (GOP)...\n");
    Print(L"[*] Setting resolution to 1920x1080 32bpp...\n");
    Print(L"[*] Loading Unknown OS Kernel into Long Mode...\n");
    Print(L"[*] Transferring execution to kernel_main()...\n");

    return EFI_SUCCESS;
}
