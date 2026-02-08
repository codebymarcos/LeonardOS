// LeonardOS - Comando: mem
// Exibe estatísticas detalhadas da memória física (PMM)

#include "cmd_mem.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../memory/pmm.h"
#include "../memory/heap.h"

void cmd_mem(const char *args) {
    (void)args;

    struct pmm_stats stats = pmm_get_stats();

    vga_puts_color("\n", THEME_DEFAULT);
    vga_puts_color("╔══════════════════════════════════════╗\n", THEME_BORDER);
    vga_puts_color("║", THEME_BORDER);
    vga_puts_color("    Memoria Fisica (PMM)              ", THEME_TITLE);
    vga_puts_color("║\n", THEME_BORDER);
    vga_puts_color("╚══════════════════════════════════════╝\n", THEME_BORDER);

    // RAM total
    vga_puts_color("  RAM total:      ", THEME_LABEL);
    vga_putint(stats.total_memory_kb / 1024);
    vga_puts_color(" MB (", THEME_DIM);
    vga_putint(stats.total_memory_kb);
    vga_puts_color(" KB)\n", THEME_DIM);

    // Frames totais
    vga_puts_color("  Frames totais:  ", THEME_LABEL);
    vga_set_color(THEME_VALUE);
    vga_putint(stats.total_frames);
    vga_puts_color("  (4KB cada)\n", THEME_DIM);

    // Frames livres
    vga_puts_color("  Frames livres:  ", THEME_LABEL);
    vga_set_color(THEME_BOOT_OK);
    vga_putint(stats.free_frames);
    vga_puts_color("  (", THEME_DIM);
    vga_putint(stats.free_memory_kb / 1024);
    vga_puts_color(" MB)\n", THEME_DIM);

    // Frames usados
    vga_puts_color("  Frames usados:  ", THEME_LABEL);
    vga_set_color(THEME_BOOT_FAIL);
    vga_putint(stats.used_frames);
    vga_puts_color("  (", THEME_DIM);
    vga_putint(stats.used_memory_kb / 1024);
    vga_puts_color(" MB)\n", THEME_DIM);

    // Frames do kernel
    vga_puts_color("  Kernel:         ", THEME_LABEL);
    vga_set_color(THEME_INFO);
    vga_putint(stats.kernel_frames);
    vga_puts_color(" frames (", THEME_DIM);
    vga_putint(stats.kernel_frames * 4);
    vga_puts_color(" KB)\n", THEME_DIM);

    // Barra visual de uso
    vga_puts_color("\n  Uso: [", THEME_LABEL);
    int bar_width = 30;
    int used_bar = 0;
    if (stats.total_frames > 0) {
        used_bar = (int)((uint32_t)bar_width * stats.used_frames / stats.total_frames);
    }
    if (used_bar > bar_width) used_bar = bar_width;

    for (int i = 0; i < bar_width; i++) {
        if (i < used_bar) {
            vga_puts_color("█", THEME_BOOT_FAIL);
        } else {
            vga_puts_color("░", THEME_BOOT_OK);
        }
    }
    vga_puts_color("] ", THEME_LABEL);

    // Percentual
    int pct = 0;
    if (stats.total_frames > 0) {
        pct = (int)((uint32_t)100 * stats.used_frames / stats.total_frames);
    }
    vga_putint(pct);
    vga_puts_color("%\n", THEME_DIM);

    // ── Heap ──
    struct heap_stats hs = heap_get_stats();

    vga_puts_color("\n", THEME_DEFAULT);
    vga_puts_color("╔══════════════════════════════════════╗\n", THEME_BORDER);
    vga_puts_color("║", THEME_BORDER);
    vga_puts_color("    Heap do Kernel                    ", THEME_TITLE);
    vga_puts_color("║\n", THEME_BORDER);
    vga_puts_color("╚══════════════════════════════════════╝\n", THEME_BORDER);

    vga_puts_color("  Tamanho total:  ", THEME_LABEL);
    vga_set_color(THEME_VALUE);
    vga_putint(hs.total_bytes);
    vga_puts_color(" bytes (", THEME_DIM);
    vga_putint(hs.pages_allocated);
    vga_puts_color(" paginas)\n", THEME_DIM);

    vga_puts_color("  Usado:          ", THEME_LABEL);
    vga_set_color(THEME_BOOT_FAIL);
    vga_putint(hs.used_bytes);
    vga_puts_color(" bytes (", THEME_DIM);
    vga_putint(hs.used_blocks);
    vga_puts_color(" blocos)\n", THEME_DIM);

    vga_puts_color("  Livre:          ", THEME_LABEL);
    vga_set_color(THEME_BOOT_OK);
    vga_putint(hs.free_bytes);
    vga_puts_color(" bytes (", THEME_DIM);
    vga_putint(hs.free_blocks);
    vga_puts_color(" blocos)\n", THEME_DIM);

    vga_puts_color("  kmalloc/kfree:  ", THEME_LABEL);
    vga_set_color(THEME_VALUE);
    vga_putint(hs.alloc_count);
    vga_puts_color(" / ", THEME_DIM);
    vga_putint(hs.free_count);
    vga_putchar('\n');

    // Barra visual do heap
    vga_puts_color("\n  Uso: [", THEME_LABEL);
    int hbar_width = 30;
    int hused_bar = 0;
    if (hs.total_bytes > 0) {
        hused_bar = (int)((uint32_t)hbar_width * hs.used_bytes / (hs.used_bytes + hs.free_bytes));
    }
    if (hused_bar > hbar_width) hused_bar = hbar_width;

    for (int i = 0; i < hbar_width; i++) {
        if (i < hused_bar) {
            vga_puts_color("█", THEME_BOOT_FAIL);
        } else {
            vga_puts_color("░", THEME_BOOT_OK);
        }
    }
    vga_puts_color("] ", THEME_LABEL);

    int hpct = 0;
    if (hs.used_bytes + hs.free_bytes > 0) {
        hpct = (int)((uint32_t)100 * hs.used_bytes / (hs.used_bytes + hs.free_bytes));
    }
    vga_putint(hpct);
    vga_puts_color("%\n\n", THEME_DIM);

    vga_set_color(THEME_DEFAULT);
}
