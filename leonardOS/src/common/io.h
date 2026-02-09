// LeonardOS - Port I/O inline functions
// Funções para ler/escrever portas de hardware x86

#ifndef __LEONARDOS_IO_H__
#define __LEONARDOS_IO_H__

#include "types.h"

// Escreve um byte numa porta I/O
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Escreve uma word (16 bits) numa porta I/O
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// Lê um byte de uma porta I/O
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Lê uma word (16 bits) de uma porta I/O
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Lê múltiplas words de uma porta I/O (insw)
static inline void insw(uint16_t port, void *buf, uint32_t count) {
    asm volatile("rep insw"
                 : "+D"(buf), "+c"(count)
                 : "d"(port)
                 : "memory");
}

// Escreve múltiplas words numa porta I/O (outsw)
static inline void outsw(uint16_t port, const void *buf, uint32_t count) {
    asm volatile("rep outsw"
                 : "+S"(buf), "+c"(count)
                 : "d"(port)
                 : "memory");
}

// Espera um ciclo de I/O (para dar tempo ao hardware)
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
