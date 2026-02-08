// LeonardOS - VMM (Virtual Memory Manager) / Paging
// Identity mapping com Page Directory + Page Tables (4KB pages)
//
// Mapeia virtual == physical nos primeiros 16MB.
// Usa PMM para alocar frames das estruturas de paging.
// API: paging_init, map_page, unmap_page, get_physical_addr

#ifndef __VMM_H__
#define __VMM_H__

#include "../common/types.h"

// ============================================================
// Constantes de paging x86
// ============================================================

// Tamanho de uma página (4KB)
#define PAGE_SIZE 4096

// Entradas por tabela (Page Directory e Page Table)
#define PAGE_ENTRIES 1024

// Tamanho coberto por uma Page Table (4MB = 1024 * 4KB)
#define PAGE_TABLE_COVERAGE (PAGE_ENTRIES * PAGE_SIZE)

// Identity mapping: quantos MB mapear no boot
#define PAGING_IDENTITY_MAP_MB 16

// Flags de entrada (bits 0-11 do PDE/PTE)
#define PAGE_PRESENT   0x001   // Página presente na memória
#define PAGE_RW        0x002   // Leitura/Escrita (0 = somente leitura)
#define PAGE_USER      0x004   // Acessível pelo user mode
#define PAGE_WT        0x008   // Write-through caching
#define PAGE_NOCACHE   0x010   // Cache desabilitado
#define PAGE_ACCESSED  0x020   // CPU acessou esta página
#define PAGE_DIRTY     0x040   // CPU escreveu nesta página (PTE only)
#define PAGE_SIZE_4MB  0x080   // Página de 4MB (PDE only, não usamos)

// Flags padrão para kernel: presente + leitura/escrita
#define PAGE_KERNEL    (PAGE_PRESENT | PAGE_RW)

// Máscara para extrair endereço base (bits 12-31)
#define PAGE_ADDR_MASK 0xFFFFF000

// ============================================================
// Extração de índices de um endereço virtual
// ============================================================

// Índice no Page Directory (bits 22-31)
#define PAGE_DIR_INDEX(addr)   (((uint32_t)(addr) >> 22) & 0x3FF)

// Índice na Page Table (bits 12-21)
#define PAGE_TABLE_INDEX(addr) (((uint32_t)(addr) >> 12) & 0x3FF)

// Offset dentro da página (bits 0-11)
#define PAGE_OFFSET(addr)      ((uint32_t)(addr) & 0xFFF)

// ============================================================
// Estatísticas do VMM
// ============================================================
struct vmm_stats {
    uint32_t pages_mapped;      // Total de páginas mapeadas
    uint32_t page_tables_used;  // Page Tables alocadas
    uint32_t page_faults;       // Page faults ocorridos
    uint32_t identity_map_mb;   // MB do identity mapping
};

// ============================================================
// API pública
// ============================================================

// Inicializa paging com identity mapping
// Deve ser chamada DEPOIS de pmm_init() e ANTES de sti
void paging_init(void);

// Mapeia uma página virtual para um frame físico
// virtual_addr e physical_addr devem ser alinhados a 4KB
// flags: PAGE_PRESENT | PAGE_RW | PAGE_USER etc.
void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);

// Remove o mapeamento de uma página virtual
void unmap_page(uint32_t virtual_addr);

// Retorna o endereço físico mapeado para um endereço virtual
// Retorna 0 se a página não está mapeada
uint32_t get_physical_addr(uint32_t virtual_addr);

// Verifica se uma página virtual está mapeada
bool is_page_mapped(uint32_t virtual_addr);

// Retorna estatísticas atuais
struct vmm_stats paging_get_stats(void);

#endif
