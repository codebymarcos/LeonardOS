// LeonardOS - Kernel principal
// Inicializa sistema e salta para shell

#include "drivers/vga/vga.h"
#include "common/colors.h"
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
    
    vga_puts_color("Bootloader: ", THEME_LABEL);
    vga_puts_color("GRUB (Multiboot2 32-bit)\n", THEME_VALUE);
    vga_puts_color("Arquitetura: ", THEME_LABEL);
    vga_puts_color("x86_32\n\n", THEME_VALUE);
    
    vga_puts_color("Iniciando shell...\n\n", THEME_BOOT);
    shell_loop();
    
    asm volatile("cli; hlt");
}
