// LeonardOS - Comando: mem
// Exibe estatísticas detalhadas da memória (PMM e Heap)

#include "cmd_mem.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../memory/pmm.h"
#include "../memory/heap.h"

// ============================================================
// Helper: Desenha barra de progresso visual
// ============================================================
static void draw_progress_bar(int used, int total, int width) {
    if (total <= 0) return;
    
    int filled = 0;
    if (total > 0) {
        filled = (int)((uint32_t)width * used / total);
    }
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

// ============================================================
// Helper: Seção com cabeçalho
// ============================================================
static void section_header(const char *title) {
    vga_puts_color("\n╔══════════════════════════════════════╗\n", THEME_BORDER);
    vga_puts_color("║ ", THEME_BORDER);
    vga_puts_color(title, THEME_TITLE);
    vga_puts_color(" ║\n", THEME_BORDER);
    vga_puts_color("╚══════════════════════════════════════╝\n", THEME_BORDER);
}

// ============================================================
// Comando mem
// ============================================================
void cmd_mem(const char *args) {
    (void)args;

    struct pmm_stats stats = pmm_get_stats();

    section_header("Memoria Fisica (PMM)");

    vga_puts_color("  RAM total:     ", THEME_LABEL);
    vga_putint(stats.total_memory_kb / 1024);
    vga_puts_color(" MB\n", THEME_DIM);

    vga_puts_color("  Frames:        ", THEME_LABEL);
    vga_putint(stats.total_frames);
    vga_puts_color(" total (4KB cada)\n", THEME_DIM);

    vga_puts_color("  Livres:        ", THEME_LABEL);
    vga_set_color(THEME_BOOT_OK);
    vga_putint(stats.free_frames);
    vga_puts_color(" (", THEME_DIM);
    vga_putint(stats.free_memory_kb / 1024);
    vga_puts_color(" MB)\n", THEME_DIM);

    vga_puts_color("  Usados:        ", THEME_LABEL);
    vga_set_color(THEME_BOOT_FAIL);
    vga_putint(stats.used_frames);
    vga_puts_color(" (", THEME_DIM);
    vga_putint(stats.used_memory_kb / 1024);
    vga_puts_color(" MB)\n", THEME_DIM);

    vga_puts_color("  Kernel:        ", THEME_LABEL);
    vga_set_color(THEME_INFO);
    vga_putint(stats.kernel_frames);
    vga_puts_color(" frames\n", THEME_DIM);

    vga_puts_color("  Uso:           ", THEME_LABEL);
    draw_progress_bar(stats.used_frames, stats.total_frames, 24);


    // ── Heap ──
    struct heap_stats hs = heap_get_stats();

    section_header("Heap do Kernel");

    vga_puts_color("  Total:         ", THEME_LABEL);
    vga_putint(hs.total_bytes);
    vga_puts_color(" bytes (", THEME_DIM);
    vga_putint(hs.pages_allocated);
    vga_puts_color(" paginas)\n", THEME_DIM);

    vga_puts_color("  Usado:         ", THEME_LABEL);
    vga_set_color(THEME_BOOT_FAIL);
    vga_putint(hs.used_bytes);
    vga_puts_color(" bytes (", THEME_DIM);
    vga_putint(hs.used_blocks);
    vga_puts_color(" blocos)\n", THEME_DIM);

    vga_puts_color("  Livre:         ", THEME_LABEL);
    vga_set_color(THEME_BOOT_OK);
    vga_putint(hs.free_bytes);
    vga_puts_color(" bytes (", THEME_DIM);
    vga_putint(hs.free_blocks);
    vga_puts_color(" blocos)\n", THEME_DIM);

    vga_puts_color("  kmalloc/kfree: ", THEME_LABEL);
    vga_putint(hs.alloc_count);
    vga_puts_color(" / ", THEME_DIM);
    vga_putint(hs.free_count);
    vga_puts_color("\n", THEME_DIM);

    vga_puts_color("  Uso:           ", THEME_LABEL);
    int total_heap = hs.used_bytes + hs.free_bytes;
    draw_progress_bar(hs.used_bytes, total_heap, 24);

    vga_puts_color("\n", THEME_DEFAULT);
}
