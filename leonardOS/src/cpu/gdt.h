// LeonardOS - GDT (Global Descriptor Table)
// Define os segmentos de memória do modo protegido x86

#ifndef __GDT_H__
#define __GDT_H__

#include "../common/types.h"

// Seletores de segmento (offset na GDT, em bytes)
#define GDT_NULL_SEG        0x00
#define GDT_KERNEL_CODE_SEG 0x08
#define GDT_KERNEL_DATA_SEG 0x10

// Número de entradas na GDT
#define GDT_NUM_ENTRIES 3

// Entrada da GDT (8 bytes cada)
struct gdt_entry {
    uint16_t limit_low;    // Limit bits 0-15
    uint16_t base_low;     // Base bits 0-15
    uint8_t  base_middle;  // Base bits 16-23
    uint8_t  access;       // Access byte
    uint8_t  granularity;  // Limit bits 16-19 + flags
    uint8_t  base_high;    // Base bits 24-31
} __attribute__((packed));

// Ponteiro da GDT (passado para lgdt)
struct gdt_ptr {
    uint16_t limit;  // Tamanho da tabela - 1
    uint32_t base;   // Endereço base da tabela
} __attribute__((packed));

// Inicializa a GDT do kernel
void gdt_init(void);

#endif
