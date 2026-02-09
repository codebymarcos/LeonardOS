// LeonardOS - PIT (Programmable Interval Timer) 8253/8254
// Timer de hardware para delays precisos e scheduling
//
// Canal 0: IRQ0 (INT 32) — timer do sistema
// Frequência base: 1.193182 MHz
// Configurado para 100 Hz (tick = 10ms)

#ifndef __PIT_H__
#define __PIT_H__

#include "../../common/types.h"

// Frequência do tick (Hz)
#define PIT_HZ          100

// Milissegundos por tick
#define PIT_MS_PER_TICK (1000 / PIT_HZ)

// ============================================================
// API pública
// ============================================================

// Inicializa o PIT: configura canal 0 a 100Hz, registra IRQ0
void pit_init(void);

// Retorna ticks desde o boot (cada tick = 10ms)
uint32_t pit_get_ticks(void);

// Retorna tempo em milissegundos desde o boot (aproximado)
uint32_t pit_get_ms(void);

// Delay bloqueante com precisão de ~10ms
// Usa polling de ticks (não busy-wait de CPU)
void pit_sleep_ms(uint32_t ms);

#endif
