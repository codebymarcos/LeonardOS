// LeonardOS - IDT (Interrupt Descriptor Table)
// Configura a tabela de interrupções

#include "idt.h"
#include "gdt.h"

// Tabela IDT e ponteiro
static struct idt_entry idt_entries[IDT_NUM_ENTRIES];
static struct idt_ptr   idt_pointer;

// Implementada em isr_stub.s
extern void idt_flush(struct idt_ptr *ptr);

// Registra uma entrada na IDT
void idt_set_entry(uint8_t idx, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt_entries[idx].base_low  = handler & 0xFFFF;
    idt_entries[idx].base_high = (handler >> 16) & 0xFFFF;
    idt_entries[idx].selector  = selector;
    idt_entries[idx].zero      = 0;
    idt_entries[idx].flags     = flags;
}

// Inicializa a IDT (zera tabela e carrega)
void idt_init(void) {
    idt_pointer.limit = (sizeof(struct idt_entry) * IDT_NUM_ENTRIES) - 1;
    idt_pointer.base  = (uint32_t)&idt_entries;

    // Zera toda a tabela
    uint8_t *p = (uint8_t *)&idt_entries;
    for (uint32_t i = 0; i < sizeof(idt_entries); i++) {
        p[i] = 0;
    }

    // Carrega a IDT na CPU
    idt_flush(&idt_pointer);
}

// Apenas recarrega a IDT na CPU (sem zerar as entradas)
void idt_load(void) {
    idt_pointer.limit = (sizeof(struct idt_entry) * IDT_NUM_ENTRIES) - 1;
    idt_pointer.base  = (uint32_t)&idt_entries;
    idt_flush(&idt_pointer);
}
