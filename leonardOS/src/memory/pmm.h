// LeonardOS - PMM (Physical Memory Manager)
// Gerencia frames de 4KB de memória física usando bitmap
//
// Lê o mapa de memória do Multiboot2 para descobrir a RAM disponível.
// Marca regiões do kernel e reservadas como usadas.
// API: pmm_alloc_frame, pmm_free_frame, pmm_get_stats

#ifndef __PMM_H__
#define __PMM_H__

#include "../common/types.h"

// Tamanho de um frame (4KB)
#define PMM_FRAME_SIZE 4096

// Máximo de memória suportada: 256MB = 65536 frames
// Bitmap: 65536 bits = 8192 bytes = 8KB
#define PMM_MAX_MEMORY_MB  256
#define PMM_MAX_FRAMES     ((PMM_MAX_MEMORY_MB * 1024 * 1024) / PMM_FRAME_SIZE)
#define PMM_BITMAP_SIZE    (PMM_MAX_FRAMES / 8)

// ============================================================
// Estruturas Multiboot2 (simplificadas, só o que precisamos)
// ============================================================

// Tag header genérica do Multiboot2
struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

// Tag tipo 6: Memory Map
#define MB2_TAG_TYPE_MMAP  6
#define MB2_TAG_TYPE_END   0

// Entrada do memory map
struct mb2_mmap_entry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;       // 1 = Available, 2 = Reserved, 3 = ACPI, etc.
    uint32_t reserved;
} __attribute__((packed));

// Tag do memory map
struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    // entries[] seguem aqui
} __attribute__((packed));

// Tipos de região de memória
#define MB2_MMAP_AVAILABLE 1
#define MB2_MMAP_RESERVED  2
#define MB2_MMAP_ACPI      3
#define MB2_MMAP_HIBERNATE 4
#define MB2_MMAP_DEFECTIVE 5

// ============================================================
// Estatísticas do PMM
// ============================================================
struct pmm_stats {
    uint32_t total_frames;     // Total de frames disponíveis
    uint32_t used_frames;      // Frames em uso
    uint32_t free_frames;      // Frames livres
    uint32_t total_memory_kb;  // RAM total em KB
    uint32_t free_memory_kb;   // RAM livre em KB
    uint32_t used_memory_kb;   // RAM usada em KB
    uint32_t kernel_frames;    // Frames do kernel
};

// ============================================================
// API pública
// ============================================================

// Inicializa o PMM usando informações do Multiboot2
// multiboot_info: ponteiro passado pelo GRUB (EBX no boot)
void pmm_init(void *multiboot_info);

// Aloca um frame físico de 4KB
// Retorna endereço físico do frame, ou 0 se não há memória
uint32_t pmm_alloc_frame(void);

// Libera um frame físico previamente alocado
void pmm_free_frame(uint32_t frame_addr);

// Verifica se um frame está em uso
bool pmm_is_frame_used(uint32_t frame_addr);

// Retorna estatísticas atuais
struct pmm_stats pmm_get_stats(void);

#endif
