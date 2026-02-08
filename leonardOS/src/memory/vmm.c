// LeonardOS - VMM (Virtual Memory Manager) / Paging
// Identity mapping x86 com two-level paging (PD → PT → Page)
//
// Inicialização:
//   1. Aloca Page Directory (1 frame) via PMM
//   2. Aloca Page Tables necessárias via PMM
//   3. Preenche identity map (virtual == physical) para 16MB
//   4. Carrega CR3 com endereço do Page Directory
//   5. Seta bit 31 (PG) de CR0 para habilitar paging
//   6. Registra page fault handler (INT 14)

#include "vmm.h"
#include "pmm.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../cpu/isr.h"

// ============================================================
// Estado interno do VMM
// ============================================================

// Page Directory: array de 1024 entradas de 32 bits
// Cada entrada aponta para uma Page Table
static uint32_t *page_directory = NULL;

// Estatísticas
static uint32_t vmm_pages_mapped = 0;
static uint32_t vmm_page_tables_used = 0;
static uint32_t vmm_page_faults = 0;

static bool vmm_initialized = false;

// ============================================================
// Helpers inline ASM
// ============================================================

// Carrega o endereço do Page Directory no CR3
static inline void load_cr3(uint32_t pd_addr) {
    asm volatile("mov %0, %%cr3" :: "r"(pd_addr) : "memory");
}

// Lê CR3 (endereço do Page Directory atual)
static inline uint32_t read_cr3(void) {
    uint32_t val;
    asm volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

// Habilita paging setando bit 31 (PG) de CR0
static inline void enable_paging(void) {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1 << 31);  // PG bit
    asm volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

// Lê CR2 (endereço virtual que causou page fault)
static inline uint32_t read_cr2(void) {
    uint32_t val;
    asm volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

// Invalida uma entrada no TLB para um endereço virtual
static inline void invlpg(uint32_t addr) {
    asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

// ============================================================
// Zera um frame (4096 bytes = 1024 uint32_t)
// ============================================================
static void zero_frame(uint32_t *frame) {
    for (int i = 0; i < 1024; i++) {
        frame[i] = 0;
    }
}

// ============================================================
// Page Fault Handler (INT 14)
// ============================================================
static void page_fault_handler(struct isr_frame *frame) {
    uint32_t fault_addr = read_cr2();
    vmm_page_faults++;

    // Error code bits:
    // bit 0: 0 = page not present, 1 = protection violation
    // bit 1: 0 = read, 1 = write
    // bit 2: 0 = kernel mode, 1 = user mode
    uint32_t err = frame->err_code;

    vga_puts_color("\n[PAGE FAULT] ", THEME_BOOT_FAIL);
    vga_puts_color("Endereco: 0x", THEME_ERROR);
    vga_puthex(fault_addr);

    vga_puts_color(" (", THEME_DIM);
    vga_puts_color(err & 0x1 ? "protecao" : "nao presente", THEME_ERROR);
    vga_puts_color(", ", THEME_DIM);
    vga_puts_color(err & 0x2 ? "escrita" : "leitura", THEME_ERROR);
    vga_puts_color(", ", THEME_DIM);
    vga_puts_color(err & 0x4 ? "user" : "kernel", THEME_ERROR);
    vga_puts_color(")\n", THEME_DIM);

    vga_puts_color("  EIP: 0x", THEME_LABEL);
    vga_puthex(frame->eip);
    vga_puts_color("  CS: 0x", THEME_LABEL);
    vga_puthex(frame->cs);
    vga_putchar('\n');

    // Halt — page fault no kernel é fatal (por enquanto)
    vga_puts_color("  Sistema parado.\n", THEME_ERROR);
    asm volatile("cli; hlt");
}

// ============================================================
// paging_init — Identity mapping dos primeiros 16MB
// ============================================================
void paging_init(void) {
    // 1. Aloca frame para o Page Directory
    uint32_t pd_phys = pmm_alloc_frame();
    if (pd_phys == 0) {
        vga_puts_color("[FAIL] ", THEME_BOOT_FAIL);
        vga_puts_color("VMM: sem memoria para Page Directory!\n", THEME_ERROR);
        return;
    }

    page_directory = (uint32_t *)pd_phys;
    zero_frame(page_directory);

    // 2. Identity map: 16MB = 4 Page Tables (cada PT cobre 4MB)
    uint32_t num_tables = PAGING_IDENTITY_MAP_MB / 4;

    for (uint32_t t = 0; t < num_tables; t++) {
        // Aloca frame para esta Page Table
        uint32_t pt_phys = pmm_alloc_frame();
        if (pt_phys == 0) {
            vga_puts_color("[FAIL] ", THEME_BOOT_FAIL);
            vga_puts_color("VMM: sem memoria para Page Table!\n", THEME_ERROR);
            return;
        }

        uint32_t *page_table = (uint32_t *)pt_phys;
        zero_frame(page_table);

        // Preenche as 1024 entradas desta Page Table
        // Cada entrada mapeia um frame de 4KB
        for (uint32_t p = 0; p < PAGE_ENTRIES; p++) {
            uint32_t phys_addr = (t * PAGE_TABLE_COVERAGE) + (p * PAGE_SIZE);
            page_table[p] = phys_addr | PAGE_KERNEL;
            vmm_pages_mapped++;
        }

        // Registra a Page Table no Page Directory
        page_directory[t] = pt_phys | PAGE_KERNEL;
        vmm_page_tables_used++;
    }

    // 3. Registra o page fault handler ANTES de habilitar paging
    isr_register_handler(ISR_PAGE_FAULT, page_fault_handler);

    // 4. Carrega CR3 e habilita paging
    load_cr3(pd_phys);
    enable_paging();

    vmm_initialized = true;
}

// ============================================================
// map_page — Mapeia uma página virtual para um frame físico
// ============================================================
void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    if (!vmm_initialized) return;

    uint32_t pd_idx = PAGE_DIR_INDEX(virtual_addr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virtual_addr);

    uint32_t *pt;

    if (page_directory[pd_idx] & PAGE_PRESENT) {
        // Page Table já existe
        pt = (uint32_t *)(page_directory[pd_idx] & PAGE_ADDR_MASK);
    } else {
        // Precisa alocar nova Page Table
        uint32_t pt_phys = pmm_alloc_frame();
        if (pt_phys == 0) return;

        pt = (uint32_t *)pt_phys;
        zero_frame(pt);

        page_directory[pd_idx] = pt_phys | PAGE_KERNEL;
        vmm_page_tables_used++;
    }

    // Mapeia a entrada
    pt[pt_idx] = (physical_addr & PAGE_ADDR_MASK) | (flags & 0xFFF);
    vmm_pages_mapped++;

    // Invalida TLB para este endereço
    invlpg(virtual_addr);
}

// ============================================================
// unmap_page — Remove mapeamento de uma página virtual
// ============================================================
void unmap_page(uint32_t virtual_addr) {
    if (!vmm_initialized) return;

    uint32_t pd_idx = PAGE_DIR_INDEX(virtual_addr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virtual_addr);

    // Verifica se a Page Table existe
    if (!(page_directory[pd_idx] & PAGE_PRESENT)) return;

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & PAGE_ADDR_MASK);

    // Verifica se a página estava mapeada
    if (pt[pt_idx] & PAGE_PRESENT) {
        pt[pt_idx] = 0;
        if (vmm_pages_mapped > 0) vmm_pages_mapped--;

        // Invalida TLB
        invlpg(virtual_addr);
    }
}

// ============================================================
// get_physical_addr — Traduz virtual → físico via page walk
// ============================================================
uint32_t get_physical_addr(uint32_t virtual_addr) {
    if (!vmm_initialized) return 0;

    uint32_t pd_idx = PAGE_DIR_INDEX(virtual_addr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virtual_addr);

    // Page Table não existe
    if (!(page_directory[pd_idx] & PAGE_PRESENT)) return 0;

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & PAGE_ADDR_MASK);

    // Página não mapeada
    if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;

    // Endereço físico = base do frame + offset
    return (pt[pt_idx] & PAGE_ADDR_MASK) | PAGE_OFFSET(virtual_addr);
}

// ============================================================
// is_page_mapped — Verifica se uma página virtual está mapeada
// ============================================================
bool is_page_mapped(uint32_t virtual_addr) {
    if (!vmm_initialized) return false;

    uint32_t pd_idx = PAGE_DIR_INDEX(virtual_addr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virtual_addr);

    if (!(page_directory[pd_idx] & PAGE_PRESENT)) return false;

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & PAGE_ADDR_MASK);
    return (pt[pt_idx] & PAGE_PRESENT) != 0;
}

// ============================================================
// paging_get_stats — Retorna estatísticas do VMM
// ============================================================
struct vmm_stats paging_get_stats(void) {
    struct vmm_stats stats;
    stats.pages_mapped = vmm_pages_mapped;
    stats.page_tables_used = vmm_page_tables_used;
    stats.page_faults = vmm_page_faults;
    stats.identity_map_mb = PAGING_IDENTITY_MAP_MB;
    return stats;
}
