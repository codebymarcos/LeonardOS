// LeonardOS - GDT (Global Descriptor Table)
// Configura segmentos flat de 4GB para modo protegido

#include "gdt.h"

// Tabela GDT e ponteiro
static struct gdt_entry gdt_entries[GDT_NUM_ENTRIES];
static struct gdt_ptr   gdt_pointer;

// Implementada em gdt_flush.s
extern void gdt_flush(struct gdt_ptr *ptr);

// Configura uma entrada na GDT
static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt_entries[idx].base_low    = (base & 0xFFFF);
    gdt_entries[idx].base_middle = (base >> 16) & 0xFF;
    gdt_entries[idx].base_high   = (base >> 24) & 0xFF;

    gdt_entries[idx].limit_low   = (limit & 0xFFFF);
    gdt_entries[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);

    gdt_entries[idx].access      = access;
}

// Inicializa a GDT com 3 segmentos flat
void gdt_init(void) {
    gdt_pointer.limit = (sizeof(struct gdt_entry) * GDT_NUM_ENTRIES) - 1;
    gdt_pointer.base  = (uint32_t)&gdt_entries;

    // Entrada 0: Null segment (obrigatório pela CPU)
    gdt_set_entry(0, 0, 0, 0, 0);

    // Entrada 1: Kernel Code Segment
    //   Base=0, Limit=4GB (0xFFFFF com granularidade 4KB)
    //   Access 0x9A: Present(1) | DPL=0(00) | Type=1 | Exec(1) | Conform(0) | Read(1) | Accessed(0)
    //   Gran   0xCF: Granularity 4KB(1) | 32-bit(1) | 0 | 0 | Limit[19:16]=F
    gdt_set_entry(1, 0x00000000, 0xFFFFF, 0x9A, 0xCF);

    // Entrada 2: Kernel Data Segment
    //   Base=0, Limit=4GB
    //   Access 0x92: Present(1) | DPL=0(00) | Type=1 | Exec(0) | Direction(0) | Write(1) | Accessed(0)
    //   Gran   0xCF: mesma granularidade
    gdt_set_entry(2, 0x00000000, 0xFFFFF, 0x92, 0xCF);

    // Carrega a GDT na CPU e recarrega segment registers
    gdt_flush(&gdt_pointer);
}
