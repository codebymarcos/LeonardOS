// LeonardOS - PIC (Programmable Interrupt Controller) 8259
// Remapeia e controla as IRQs de hardware

#ifndef __PIC_H__
#define __PIC_H__

#include "../../common/types.h"

// Inicializa e remapeia o PIC (IRQs 0-15 -> INT 32-47)
void pic_init(void);

// Envia End-Of-Interrupt para o PIC
void pic_send_eoi(uint8_t irq);

// Habilita (unmask) uma IRQ específica
void pic_unmask_irq(uint8_t irq);

// Desabilita (mask) uma IRQ específica
void pic_mask_irq(uint8_t irq);

#endif
