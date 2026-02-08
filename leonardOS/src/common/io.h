// LeonardOS - Port I/O inline functions
// Funções para ler/escrever portas de hardware x86

#ifndef __LEONARDOS_IO_H__
#define __LEONARDOS_IO_H__

#include "types.h"

// Escreve um byte numa porta I/O
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Lê um byte de uma porta I/O
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Espera um ciclo de I/O (para dar tempo ao hardware)
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
