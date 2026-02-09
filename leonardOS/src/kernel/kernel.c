// LeonardOS - Kernel principal
// Inicializa sistema e salta para shell

#include "drivers/vga/vga.h"
#include "common/colors.h"
#include "cpu/gdt.h"
#include "cpu/isr.h"
#include "drivers/pic/pic.h"
#include "drivers/keyboard/keyboard.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "shell/shell.h"

void __attribute__((regparm(0))) kernel_main_32(unsigned int magic, void *multiboot_info) {
    // Inicializa VGA
    vga_set_color(THEME_DEFAULT);
    vga_clear();
    vga_puts_color("=== LeonardOS ===\n", THEME_BANNER);
    vga_puts_color("Kernel iniciado com sucesso.\n\n", THEME_BOOT_OK);
    
    // Verifica magic number
    if (magic != 0x36d76289) {
        vga_puts_color("ERRO: ", THEME_BOOT_FAIL);
        vga_puts_color("Magic number invalido: ", THEME_ERROR);
        vga_puthex((unsigned long)magic);
        vga_puts("\n");
        vga_puts_color("Kernel nao foi carregado pelo GRUB?\n", THEME_ERROR);
        asm volatile("cli; hlt");
        while (1);
    }
    
    // Inicializa GDT
    gdt_init();
    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("GDT carregada\n", THEME_BOOT);

    // Inicializa PIC (remapeia IRQs para INT 32-47)
    pic_init();
    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("PIC remapeado\n", THEME_BOOT);

    // Inicializa ISRs e IDT
    isr_init();
    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("IDT carregada (ISR + IRQ)\n", THEME_BOOT);

    // Inicializa teclado (IRQ1)
    kbd_init();
    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("Teclado PS/2 (IRQ1)\n", THEME_BOOT);

    // Inicializa PMM (Physical Memory Manager)
    pmm_init(multiboot_info);
    {
        struct pmm_stats stats = pmm_get_stats();
        vga_puts_color("[OK] ", THEME_BOOT_OK);
        vga_puts_color("PMM: ", THEME_BOOT);
        vga_putint(stats.total_memory_kb / 1024);
        vga_puts_color("MB detectados, ", THEME_BOOT);
        vga_putint(stats.free_frames);
        vga_puts_color(" frames livres\n", THEME_BOOT);
    }

    // Inicializa Paging (VMM) - Identity mapping 16MB
    paging_init();
    {
        struct vmm_stats vs = paging_get_stats();
        vga_puts_color("[OK] ", THEME_BOOT_OK);
        vga_puts_color("Paging: identity map ", THEME_BOOT);
        vga_putint(vs.identity_map_mb);
        vga_puts_color("MB, ", THEME_BOOT);
        vga_putint(vs.page_tables_used);
        vga_puts_color(" page tables\n", THEME_BOOT);
    }

    // Inicializa Heap do Kernel (kmalloc/kfree)
    heap_init();
    {
        struct heap_stats hs = heap_get_stats();
        vga_puts_color("[OK] ", THEME_BOOT_OK);
        vga_puts_color("Heap: ", THEME_BOOT);
        vga_putint(hs.pages_allocated * 4);
        vga_puts_color("KB inicial, ", THEME_BOOT);
        vga_putint(hs.free_bytes);
        vga_puts_color(" bytes livres\n", THEME_BOOT);
    }

    // Inicializa VFS + RamFS
    vfs_init();
    vfs_node_t *ramfs_root = ramfs_init();
    vfs_mount_root(ramfs_root);
    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("VFS + RamFS montado em /\n", THEME_BOOT);

    // Habilita interrupções
    asm volatile("sti");
    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("Interrupcoes habilitadas\n", THEME_BOOT);

    vga_puts("\n");
    vga_puts_color("Bootloader: ", THEME_LABEL);
    vga_puts_color("GRUB (Multiboot2 32-bit)\n", THEME_VALUE);
    vga_puts_color("Arquitetura: ", THEME_LABEL);
    vga_puts_color("x86_32\n\n", THEME_VALUE);
    
    vga_puts_color("Iniciando shell...\n\n", THEME_BOOT);
    shell_loop();
    
    asm volatile("cli; hlt");
}
