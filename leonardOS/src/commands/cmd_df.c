// LeonardOS - Comando: df
// Exibe uso de espaço em disco (filesystems)

#include "cmd_df.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../fs/leonfs.h"
#include "../fs/vfs.h"

// Helper: Desenha barra de uso
static void draw_bar(int used, int total, int width) {
    if (total <= 0) return;
    
    int filled = (int)((uint32_t)width * used / total);
    if (filled > width) filled = width;

    vga_puts_color("[", THEME_LABEL);
    for (int i = 0; i < width; i++) {
        vga_puts_color(i < filled ? "█" : "░", i < filled ? THEME_BOOT_FAIL : THEME_BOOT_OK);
    }
    vga_puts_color("] ", THEME_LABEL);

    int pct = total > 0 ? (int)((uint32_t)100 * used / total) : 0;
    vga_putint(pct);
    vga_puts_color("%\n", THEME_DIM);
}

void cmd_df(const char *args) {
    (void)args;

    vga_puts_color("\n", THEME_DEFAULT);
    vga_puts_color("╔════════════════════════════════════════════════════╗\n", THEME_BORDER);
    vga_puts_color("║ ", THEME_BORDER);
    vga_puts_color("Filesystems", THEME_TITLE);
    vga_puts_color(" ║\n", THEME_BORDER);
    vga_puts_color("╚════════════════════════════════════════════════════╝\n", THEME_BORDER);

    // RamFS (sempre em /)
    vga_puts_color("  /       (RamFS)    ", THEME_INFO);
    vga_puts_color(" - RAM (dinâmico)\n", THEME_DIM);

    // LeonFS (em /mnt se disco presente)
    leonfs_superblock_t *sb = leonfs_get_superblock();
    if (sb && sb->magic == LEONFS_MAGIC) {
        uint32_t total_blocks = sb->total_blocks;
        uint32_t free_blocks = sb->free_blocks;
        uint32_t used_blocks = total_blocks - free_blocks;
        uint32_t block_size = 512;  // LEONFS_BLOCK_SIZE

        uint32_t used_kb = used_blocks * block_size / 1024;
        uint32_t total_kb = total_blocks * block_size / 1024;
        uint32_t used_mb = used_kb / 1024;
        uint32_t total_mb = total_kb / 1024;

        vga_puts_color("  /mnt    (LeonFS)   ", THEME_INFO);
        vga_putint(used_mb);
        vga_puts_color(" / ", THEME_DEFAULT);
        vga_putint(total_mb);
        vga_puts_color(" MB  (", THEME_DIM);
        vga_putint(used_kb);
        vga_puts_color(" / ", THEME_DIM);
        vga_putint(total_kb);
        vga_puts_color(" KB)  ", THEME_DIM);
        draw_bar(used_blocks, total_blocks, 20);
        
        vga_puts_color("  Inodes: ", THEME_LABEL);
        vga_putint(LEONFS_MAX_INODES - sb->free_inodes);
        vga_puts_color(" / ", THEME_DIM);
        vga_putint(LEONFS_MAX_INODES);
        vga_putchar('\n');
    } else {
        vga_puts_color("  /mnt    (LeonFS)   ", THEME_INFO);
        vga_puts_color(" - sem disco\n", THEME_WARNING);
    }

    vga_puts_color("\n", THEME_DEFAULT);
}
