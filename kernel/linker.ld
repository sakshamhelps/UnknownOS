/*
 * Unknown OS - Kernel Linker Script
 * Higher-half kernel at 0xFFFF800000000000
 * Author: byrex <byrex.me@proton.me>
 */

ENTRY(_start)

SECTIONS
{
    /* Higher-half kernel base */
    . = 0xFFFF800000000000;

    .text ALIGN(0x1000) :
    {
        *(.text.start)        /* Entry point */
        *(.text)              /* Code */
        *(.rodata*)           /* Read-only data */
        *(.eh_frame*)         /* Exception handling */
    }

    .data ALIGN(0x1000) :
    {
        *(.data)              /* Initialized data */
    }

    .bss ALIGN(0x1000) :
    {
        *(.bss)               /* Uninitialized data */
        *(COMMON)             /* Common symbols */
    }

    /* Kernel heap starts after .bss */
    . = ALIGN(0x1000);
    kernel_heap_start = .;
    . += 16M;                 /* 16 MiB initial heap */
    kernel_heap_end = .;

    /* Stack for early boot */
    . = ALIGN(0x1000);
    boot_stack_bottom = .;
    . += 64K;                 /* 64 KiB boot stack */
    boot_stack_top = .;

    /* End of kernel */
    kernel_end = .;

    /* Discard sections */
    /DISCARD/ :
    {
        *(.note.gnu.build-id)
        *(.note.GNU-stack)
        *(.comment)
    }
}