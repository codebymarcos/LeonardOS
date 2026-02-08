// LeonardOS - ISR (Interrupt Service Routines)
// Gerencia as rotinas de tratamento de interrupções

#ifndef __ISR_H__
#define __ISR_H__

#include "../common/types.h"

// Exceções da CPU (0-31)
#define ISR_DIVISION_BY_ZERO     0
#define ISR_DEBUG                1
#define ISR_NMI                  2
#define ISR_BREAKPOINT           3
#define ISR_OVERFLOW             4
#define ISR_BOUND_RANGE          5
#define ISR_INVALID_OPCODE       6
#define ISR_DEVICE_NOT_AVAILABLE 7
#define ISR_DOUBLE_FAULT         8
#define ISR_INVALID_TSS          10
#define ISR_SEGMENT_NOT_PRESENT  11
#define ISR_STACK_SEGMENT_FAULT  12
#define ISR_GENERAL_PROTECTION   13
#define ISR_PAGE_FAULT           14
#define ISR_X87_FLOATING_POINT   16
#define ISR_ALIGNMENT_CHECK      17
#define ISR_MACHINE_CHECK        18
#define ISR_SIMD_FLOATING_POINT  19

// IRQs do PIC (mapeadas para 32-47)
#define IRQ_BASE    32
#define IRQ_TIMER   0   // IRQ0 -> INT 32
#define IRQ_KEYBOARD 1  // IRQ1 -> INT 33

// Converte IRQ number para INT number
#define IRQ_TO_INT(irq) ((irq) + IRQ_BASE)

// Struct que representa o estado da CPU quando ocorre uma interrupção
// Preenchida pelo stub assembly e passada ao handler C
struct isr_frame {
    // Registradores de segmento salvos pelo stub
    uint32_t gs, fs, es, ds;
    // Registradores pushados pelo stub (pusha)
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    // Pushados pelo stub
    uint32_t int_no, err_code;
    // Pushados automaticamente pela CPU
    uint32_t eip, cs, eflags;
} __attribute__((packed));

// Tipo de callback para handlers de interrupção
typedef void (*isr_handler_t)(struct isr_frame *frame);

// Registra um handler para uma interrupção
void isr_register_handler(uint8_t int_no, isr_handler_t handler);

// Inicializa ISRs e registra na IDT
void isr_init(void);

#endif
