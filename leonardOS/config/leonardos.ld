/* LeonardOS - Linker Script (32-bit) */
/* Organiza memória para kernel x86 com Multiboot2 */
/* IMPORTANTE: Multiboot2 espera cabeçalho nos primeiros 8KB! */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS {
    . = 0x00100000;  /* Kernel em 1MB */
    
    /* Seção Multiboot header NO INÍCIO (primeiros 32KB do arquivo) */
    .multiboot_header ALIGN(8) : {
        *(.multiboot_header)
    }
    
    /* Código executável */
    .text ALIGN(0x1000) : {
        *(.text)
        *(.text.*)
    }
    
    /* Dados apenas-leitura */
    .rodata ALIGN(0x1000) : {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* Dados inicializados */
    .data ALIGN(0x1000) : {
        *(.data)
        *(.data.*)
    }
    
    /* Dados não inicializados (BSS) */
    .bss ALIGN(0x1000) : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    
    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.gnu_debugaltlink)
        *(.eh_frame)
    }
}
