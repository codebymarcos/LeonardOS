// LeonardOS - PMM (Physical Memory Manager)
// Bitmap allocator para frames de 4KB
//
// Usa Multiboot2 mmap para detectar RAM disponível.
// Cada bit no bitmap = 1 frame de 4KB.
// bit=0 → frame livre, bit=1 → frame em uso.

#include "pmm.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// ============================================================
// Símbolo do linker: fim do kernel na memória
// ============================================================
extern uint32_t _kernel_end;

// ============================================================
// Bitmap de frames físicos
// ============================================================
static uint8_t pmm_bitmap[PMM_BITMAP_SIZE];

// Estatísticas
static uint32_t pmm_total_frames;
static uint32_t pmm_used_frames;
static uint32_t pmm_total_memory_kb;
static uint32_t pmm_kernel_frames;

// Flag de inicialização
static bool pmm_initialized = false;

// ============================================================
// Helpers de bitmap
// ============================================================

// Marca um frame como usado
static inline void bitmap_set(uint32_t frame) {
    if (frame < PMM_MAX_FRAMES) {
        pmm_bitmap[frame / 8] |= (1 << (frame % 8));
    }
}

// Marca um frame como livre
static inline void bitmap_clear(uint32_t frame) {
    if (frame < PMM_MAX_FRAMES) {
        pmm_bitmap[frame / 8] &= ~(1 << (frame % 8));
    }
}

// Verifica se um frame está em uso
static inline bool bitmap_test(uint32_t frame) {
    if (frame >= PMM_MAX_FRAMES) return true;  // Fora do range = "usado"
    return (pmm_bitmap[frame / 8] & (1 << (frame % 8))) != 0;
}

// ============================================================
// Alinha endereço para cima ao próximo múltiplo de align
// ============================================================
static inline uint32_t align_up(uint32_t addr, uint32_t align) {
    return (addr + align - 1) & ~(align - 1);
}

// ============================================================
// pmm_init - Inicializa PMM via Multiboot2 memory map
// ============================================================
void pmm_init(void *multiboot_info) {
    // Zera bitmap (tudo "usado" por padrão para segurança)
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    pmm_total_frames = 0;
    pmm_used_frames = 0;
    pmm_total_memory_kb = 0;
    pmm_kernel_frames = 0;

    if (!multiboot_info) {
        vga_puts_color("[WARN] ", THEME_BOOT_FAIL);
        vga_puts_color("PMM: multiboot_info nulo!\n", THEME_ERROR);
        return;
    }

    // ========================================================
    // Parse das tags Multiboot2
    // ========================================================
    // Estrutura Multiboot2 info:
    //   [0..3]  total_size (uint32_t)
    //   [4..7]  reserved   (uint32_t)
    //   [8..]   tags (alinhadas a 8 bytes)
    uint32_t mb_total_size = *(uint32_t *)multiboot_info;
    (void)mb_total_size;

    // Começa no primeiro tag (offset 8)
    struct mb2_tag *tag = (struct mb2_tag *)((uint8_t *)multiboot_info + 8);

    bool found_mmap = false;

    while (tag->type != MB2_TAG_TYPE_END) {
        if (tag->type == MB2_TAG_TYPE_MMAP) {
            found_mmap = true;
            struct mb2_tag_mmap *mmap_tag = (struct mb2_tag_mmap *)tag;
            uint32_t entry_size = mmap_tag->entry_size;

            // Itera pelas entradas do mmap
            struct mb2_mmap_entry *entry = (struct mb2_mmap_entry *)((uint8_t *)mmap_tag + sizeof(struct mb2_tag_mmap));
            struct mb2_mmap_entry *end = (struct mb2_mmap_entry *)((uint8_t *)mmap_tag + mmap_tag->size);

            while (entry < end) {
                uint32_t base = (uint32_t)entry->base_addr;
                uint32_t length = (uint32_t)entry->length;

                // Contabiliza memória total
                pmm_total_memory_kb += length / 1024;

                if (entry->type == MB2_MMAP_AVAILABLE && length > 0) {
                    // Região disponível: marca frames como livres
                    uint32_t start_frame = align_up(base, PMM_FRAME_SIZE) / PMM_FRAME_SIZE;
                    uint32_t end_addr = base + length;
                    uint32_t end_frame = end_addr / PMM_FRAME_SIZE;

                    // Limita ao máximo suportado
                    if (end_frame > PMM_MAX_FRAMES) {
                        end_frame = PMM_MAX_FRAMES;
                    }

                    for (uint32_t f = start_frame; f < end_frame; f++) {
                        bitmap_clear(f);
                        pmm_total_frames++;
                    }
                }

                // Próxima entrada (usa entry_size do tag)
                entry = (struct mb2_mmap_entry *)((uint8_t *)entry + entry_size);
            }
        }

        // Próximo tag (alinhado a 8 bytes)
        uint32_t tag_size = align_up(tag->size, 8);
        tag = (struct mb2_tag *)((uint8_t *)tag + tag_size);
    }

    if (!found_mmap) {
        vga_puts_color("[WARN] ", THEME_BOOT_FAIL);
        vga_puts_color("PMM: Multiboot2 mmap nao encontrado!\n", THEME_ERROR);
        return;
    }

    // ========================================================
    // Protege regiões que não podem ser alocadas
    // ========================================================

    // 1. Protege memória baixa (0x00000 - 0xFFFFF = primeiro 1MB)
    //    Inclui IVT, BDA, VGA, ROM BIOS, etc.
    uint32_t low_frames = 0x100000 / PMM_FRAME_SIZE;  // 256 frames
    for (uint32_t f = 0; f < low_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            pmm_used_frames++;
        }
    }

    // 2. Protege o kernel (0x100000 até _kernel_end)
    uint32_t kernel_start = 0x100000;
    uint32_t kernel_end = align_up((uint32_t)(uintptr_t)&_kernel_end, PMM_FRAME_SIZE);
    uint32_t kernel_start_frame = kernel_start / PMM_FRAME_SIZE;
    uint32_t kernel_end_frame = kernel_end / PMM_FRAME_SIZE;

    pmm_kernel_frames = kernel_end_frame - kernel_start_frame;

    for (uint32_t f = kernel_start_frame; f < kernel_end_frame; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            pmm_used_frames++;
        }
    }

    // 3. Protege a estrutura Multiboot2 info
    uint32_t mb_start = (uint32_t)(uintptr_t)multiboot_info;
    uint32_t mb_end = align_up(mb_start + mb_total_size, PMM_FRAME_SIZE);
    uint32_t mb_start_frame = mb_start / PMM_FRAME_SIZE;
    uint32_t mb_end_frame = mb_end / PMM_FRAME_SIZE;

    for (uint32_t f = mb_start_frame; f < mb_end_frame; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            pmm_used_frames++;
        }
    }

    // Frame 0 nunca deve ser alocável (endereço 0 = NULL)
    if (!bitmap_test(0)) {
        bitmap_set(0);
        pmm_used_frames++;
    }

    pmm_initialized = true;
}

// ============================================================
// pmm_alloc_frame - Aloca um frame físico (first-fit)
// ============================================================
uint32_t pmm_alloc_frame(void) {
    if (!pmm_initialized) return 0;

    // Busca linear no bitmap (first-fit)
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE; i++) {
        if (pmm_bitmap[i] == 0xFF) continue;  // Byte todo usado

        // Encontra bit livre neste byte
        for (int bit = 0; bit < 8; bit++) {
            if (!(pmm_bitmap[i] & (1 << bit))) {
                uint32_t frame = i * 8 + bit;
                if (frame >= PMM_MAX_FRAMES) return 0;

                bitmap_set(frame);
                pmm_used_frames++;
                return frame * PMM_FRAME_SIZE;
            }
        }
    }

    return 0;  // Sem memória
}

// ============================================================
// pmm_free_frame - Libera um frame
// ============================================================
void pmm_free_frame(uint32_t frame_addr) {
    if (!pmm_initialized) return;

    // Valida alinhamento
    if (frame_addr % PMM_FRAME_SIZE != 0) return;

    uint32_t frame = frame_addr / PMM_FRAME_SIZE;
    if (frame >= PMM_MAX_FRAMES) return;

    // Só libera se estava em uso (previne double-free)
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        if (pmm_used_frames > 0) {
            pmm_used_frames--;
        }
    }
}

// ============================================================
// pmm_is_frame_used - Verifica se um frame está em uso
// ============================================================
bool pmm_is_frame_used(uint32_t frame_addr) {
    if (frame_addr % PMM_FRAME_SIZE != 0) return true;
    uint32_t frame = frame_addr / PMM_FRAME_SIZE;
    return bitmap_test(frame);
}

// ============================================================
// pmm_get_stats - Retorna estatísticas do PMM
// ============================================================
struct pmm_stats pmm_get_stats(void) {
    struct pmm_stats stats;
    stats.total_frames = pmm_total_frames;
    stats.used_frames = pmm_used_frames;
    stats.free_frames = pmm_total_frames > pmm_used_frames ? 
                        pmm_total_frames - pmm_used_frames : 0;
    stats.total_memory_kb = pmm_total_memory_kb;
    stats.used_memory_kb = pmm_used_frames * (PMM_FRAME_SIZE / 1024);
    stats.free_memory_kb = stats.free_frames * (PMM_FRAME_SIZE / 1024);
    stats.kernel_frames = pmm_kernel_frames;
    return stats;
}
