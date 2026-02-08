// LeonardOS - IDT (Interrupt Descriptor Table)
// Gerencia a tabela de interrupções do x86

#ifndef __IDT_H__
#define __IDT_H__

#include "../common/types.h"

// Número total de entradas na IDT (256 interrupções possíveis no x86)
#define IDT_NUM_ENTRIES 256

// Flags de tipo para entradas da IDT
#define IDT_FLAG_PRESENT    0x80   // Entrada presente
#define IDT_FLAG_DPL0       0x00   // Ring 0
#define IDT_FLAG_DPL3       0x60   // Ring 3
#define IDT_FLAG_GATE_INT32 0x0E   // 32-bit Interrupt Gate (limpa IF)
#define IDT_FLAG_GATE_TRAP  0x0F   // 32-bit Trap Gate (não limpa IF)

// Atalho: Interrupt Gate ring 0
#define IDT_GATE_KERNEL (IDT_FLAG_PRESENT | IDT_FLAG_DPL0 | IDT_FLAG_GATE_INT32)

// Entrada da IDT (8 bytes cada)
struct idt_entry {
    uint16_t base_low;     // Handler address bits 0-15
    uint16_t selector;     // Seletor de segmento de código (GDT)
    uint8_t  zero;         // Sempre 0
    uint8_t  flags;        // Type + DPL + Present
    uint16_t base_high;    // Handler address bits 16-31
} __attribute__((packed));

// Ponteiro da IDT (passado para lidt)
struct idt_ptr {
    uint16_t limit;  // Tamanho da tabela - 1
    uint32_t base;   // Endereço base da tabela
} __attribute__((packed));

// Inicializa a IDT (zera tabela e carrega)
void idt_init(void);

// Apenas recarrega a IDT na CPU (sem zerar)
void idt_load(void);

// Registra um handler na IDT
void idt_set_entry(uint8_t idx, uint32_t handler, uint16_t selector, uint8_t flags);

#endif
