// LeonardOS - Heap do Kernel (kmalloc / kfree)
// Alocador de memória dinâmica com linked-list e first-fit
//
// Usa PMM para obter frames de 4KB sob demanda.
// Dentro do identity map (0-16MB), não precisa de map_page extra.
// API: heap_init, kmalloc, kfree, heap_get_stats

#ifndef __HEAP_H__
#define __HEAP_H__

#include "../common/types.h"

// ============================================================
// Constantes do Heap
// ============================================================

// Endereço inicial do heap (5MB — dentro do identity map, acima do kernel)
#define HEAP_START        0x00500000

// Páginas iniciais alocadas no boot (4 frames = 16KB)
#define HEAP_INITIAL_PAGES 4

// Alinhamento mínimo de retorno do kmalloc (8 bytes)
#define HEAP_ALIGNMENT     8

// Tamanho de uma página (deve bater com PMM_FRAME_SIZE)
#define HEAP_PAGE_SIZE     4096

// ============================================================
// Estrutura de bloco do heap
// ============================================================
// Cada bloco tem um header seguido dos dados úteis.
// Layout: [heap_block_t header][dados do usuário...]

typedef struct heap_block {
    uint32_t size;              // Tamanho útil (sem contar o header)
    uint8_t  free;              // 1 = livre, 0 = em uso
    uint8_t  _pad[3];           // Padding interno
    struct heap_block *next;    // Próximo bloco na lista
    uint32_t _reserved;         // Pad struct para 16 bytes (alinha dados a 8)
} heap_block_t;

// Tamanho do header (deve ser múltiplo de HEAP_ALIGNMENT)
#define HEAP_HEADER_SIZE  sizeof(heap_block_t)

// ============================================================
// Estatísticas do Heap
// ============================================================
struct heap_stats {
    uint32_t total_bytes;       // Espaço total do heap (headers + dados)
    uint32_t used_bytes;        // Bytes em uso (dados, sem headers)
    uint32_t free_bytes;        // Bytes livres (dados, sem headers)
    uint32_t total_blocks;      // Número de blocos na lista
    uint32_t free_blocks;       // Blocos livres
    uint32_t used_blocks;       // Blocos em uso
    uint32_t alloc_count;       // Total de kmalloc chamados
    uint32_t free_count;        // Total de kfree chamados
    uint32_t pages_allocated;   // Páginas de 4KB alocadas para o heap
};

// ============================================================
// API pública
// ============================================================

// Inicializa o heap do kernel
// Deve ser chamada DEPOIS de paging_init() e ANTES de sti
void heap_init(void);

// Aloca size bytes de memória do kernel
// Retorna ponteiro alinhado a 8 bytes, ou NULL se falhar
void *kmalloc(uint32_t size);

// Libera memória previamente alocada com kmalloc
// Faz merge com blocos adjacentes livres (coalescing)
void kfree(void *ptr);

// Retorna estatísticas atuais do heap
struct heap_stats heap_get_stats(void);

#endif
