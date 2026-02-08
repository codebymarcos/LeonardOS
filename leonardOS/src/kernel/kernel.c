// LeonardOS - Kernel principal
// Inicializa sistema e salta para shell

#include "drivers/vga/vga.h"
#include "shell/shell.h"

void __attribute__((regparm(0))) kernel_main_32(unsigned int magic, void *multiboot_info) {
    // Inicializa VGA
    vga_clear();
    vga_puts("=== LeonardOS ===\n");
    vga_puts("Kernel iniciado com sucesso.\n\n");
    
    // Verifica magic number
    if (magic != 0x36d76289) {
        vga_puts("ERRO: Magic number invalido: ");
        vga_puthex((unsigned long)magic);
        vga_puts("\n");
        vga_puts("Kernel nao foi carregado pelo GRUB?\n");
        asm volatile("cli; hlt");
        while (1);
    }
    
    vga_puts("Bootloader: GRUB (Multiboot2 32-bit)\n");
    vga_puts("Arquitetura: x86_32\n\n");
    
    vga_puts("Iniciando shell...\n\n");
    shell_loop();
    
    asm volatile("cli; hlt");
}
