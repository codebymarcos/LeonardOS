// LeonardOS - Heap do Kernel (kmalloc / kfree)
// Linked-list allocator com first-fit e coalescing
//
// Estrutura:
//   heap_start → [block header][dados][block header][dados]...
//   Cada block header tem size, free flag, e ponteiro para next
//   kmalloc faz first-fit, split se sobrar espaço
//   kfree marca livre e faz merge com vizinhos

#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// ============================================================
// Estado interno do heap
// ============================================================

// Ponteiro para o primeiro bloco da lista
static heap_block_t *heap_head = NULL;

// Limite atual do heap (endereço do próximo byte após o heap)
static uint32_t heap_end = 0;

// Contadores
static uint32_t heap_pages_allocated = 0;
static uint32_t heap_alloc_count = 0;
static uint32_t heap_free_count = 0;

static bool heap_initialized = false;

// ============================================================
// Helpers
// ============================================================

// Alinha um valor para cima ao próximo múltiplo de align
static inline uint32_t align_up(uint32_t val, uint32_t align) {
    return (val + align - 1) & ~(align - 1);
}

// ============================================================
// heap_expand — Adiciona mais páginas ao heap
// ============================================================
static bool heap_expand(uint32_t min_bytes) {
    // Calcula quantas páginas precisamos
    uint32_t pages_needed = (min_bytes + HEAP_PAGE_SIZE - 1) / HEAP_PAGE_SIZE;
    if (pages_needed == 0) pages_needed = 1;

    uint32_t new_bytes = 0;

    for (uint32_t i = 0; i < pages_needed; i++) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0) return false;

        // Mapeia o frame físico no endereço virtual heap_end
        map_page(heap_end, frame, PAGE_KERNEL);

        heap_end += HEAP_PAGE_SIZE;
        heap_pages_allocated++;
        new_bytes += HEAP_PAGE_SIZE;
    }

    if (new_bytes == 0) return false;

    // Encontra o último bloco da lista
    heap_block_t *last = heap_head;
    while (last->next != NULL) {
        last = last->next;
    }

    if (last->free) {
        // Estende o último bloco livre
        last->size += new_bytes;
    } else {
        // Cria um novo bloco livre no espaço expandido
        heap_block_t *new_block = (heap_block_t *)((uint8_t *)last + HEAP_HEADER_SIZE + last->size);
        new_block->size = new_bytes - HEAP_HEADER_SIZE;
        new_block->free = 1;
        new_block->next = NULL;
        last->next = new_block;
    }

    return true;
}

// ============================================================
// heap_init — Inicializa o heap com páginas iniciais
// ============================================================
void heap_init(void) {
    heap_end = HEAP_START;

    // Aloca as páginas iniciais e mapeia no espaço virtual do heap
    for (uint32_t i = 0; i < HEAP_INITIAL_PAGES; i++) {
        uint32_t frame = pmm_alloc_frame();
        if (frame == 0) {
            vga_puts_color("[FAIL] ", THEME_BOOT_FAIL);
            vga_puts_color("Heap: sem memoria para pagina inicial!\n", THEME_ERROR);
            return;
        }

        // Mapeia frame físico → endereço virtual do heap
        map_page(heap_end, frame, PAGE_KERNEL);

        heap_end += HEAP_PAGE_SIZE;
        heap_pages_allocated++;
    }

    // Cria o bloco livre inicial que cobre todo o espaço
    heap_head = (heap_block_t *)HEAP_START;
    heap_head->size = (heap_end - HEAP_START) - HEAP_HEADER_SIZE;
    heap_head->free = 1;
    heap_head->next = NULL;

    heap_initialized = true;
}

// ============================================================
// kmalloc — Aloca size bytes (first-fit)
// ============================================================
void *kmalloc(uint32_t size) {
    if (!heap_initialized || size == 0) return NULL;

    // Alinha o tamanho pedido
    size = align_up(size, HEAP_ALIGNMENT);

    // First-fit: percorre a lista procurando bloco livre com espaço suficiente
    heap_block_t *current = heap_head;

    while (current != NULL) {
        if (current->free && current->size >= size) {
            // Encontrou! Verifica se vale a pena fazer split
            uint32_t remaining = current->size - size;

            if (remaining > HEAP_HEADER_SIZE + HEAP_ALIGNMENT) {
                // Split: cria um novo bloco livre no espaço que sobrou
                heap_block_t *new_block = (heap_block_t *)((uint8_t *)current + HEAP_HEADER_SIZE + size);
                new_block->size = remaining - HEAP_HEADER_SIZE;
                new_block->free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->free = 0;
            heap_alloc_count++;

            // Retorna ponteiro para a área de dados (logo após o header)
            return (void *)((uint8_t *)current + HEAP_HEADER_SIZE);
        }

        current = current->next;
    }

    // Não encontrou bloco livre — tenta expandir o heap
    uint32_t needed = size + HEAP_HEADER_SIZE;
    if (heap_expand(needed)) {
        // Tenta alocar novamente (recursão limitada a 1 nível)
        // Percorre de novo para encontrar o bloco expandido
        current = heap_head;
        while (current != NULL) {
            if (current->free && current->size >= size) {
                uint32_t remaining = current->size - size;

                if (remaining > HEAP_HEADER_SIZE + HEAP_ALIGNMENT) {
                    heap_block_t *new_block = (heap_block_t *)((uint8_t *)current + HEAP_HEADER_SIZE + size);
                    new_block->size = remaining - HEAP_HEADER_SIZE;
                    new_block->free = 1;
                    new_block->next = current->next;

                    current->size = size;
                    current->next = new_block;
                }

                current->free = 0;
                heap_alloc_count++;
                return (void *)((uint8_t *)current + HEAP_HEADER_SIZE);
            }
            current = current->next;
        }
    }

    return NULL;  // Sem memória
}

// ============================================================
// kfree — Libera memória e faz coalescing
// ============================================================
void kfree(void *ptr) {
    if (!heap_initialized || ptr == NULL) return;

    // O header está logo antes do ponteiro retornado
    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - HEAP_HEADER_SIZE);

    // Validação básica: o ponteiro deve estar dentro do heap
    uint32_t addr = (uint32_t)(uintptr_t)block;
    if (addr < HEAP_START || addr >= heap_end) return;

    // Já está livre? (double-free protection)
    if (block->free) return;

    block->free = 1;
    heap_free_count++;

    // Coalescing: merge com blocos livres adjacentes
    // Percorre toda a lista e faz merge de pares consecutivos livres
    heap_block_t *current = heap_head;
    while (current != NULL && current->next != NULL) {
        if (current->free && current->next->free) {
            // Merge: absorve o próximo bloco
            current->size += HEAP_HEADER_SIZE + current->next->size;
            current->next = current->next->next;
            // Não avança — pode ter mais merges consecutivos
        } else {
            current = current->next;
        }
    }
}

// ============================================================
// heap_get_stats — Retorna estatísticas do heap
// ============================================================
struct heap_stats heap_get_stats(void) {
    struct heap_stats stats;
    stats.total_bytes = heap_end - HEAP_START;
    stats.used_bytes = 0;
    stats.free_bytes = 0;
    stats.total_blocks = 0;
    stats.free_blocks = 0;
    stats.used_blocks = 0;
    stats.alloc_count = heap_alloc_count;
    stats.free_count = heap_free_count;
    stats.pages_allocated = heap_pages_allocated;

    heap_block_t *current = heap_head;
    while (current != NULL) {
        stats.total_blocks++;
        if (current->free) {
            stats.free_blocks++;
            stats.free_bytes += current->size;
        } else {
            stats.used_blocks++;
            stats.used_bytes += current->size;
        }
        current = current->next;
    }

    return stats;
}
