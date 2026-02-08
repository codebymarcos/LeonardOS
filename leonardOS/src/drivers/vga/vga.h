// VGA Driver - Interface pública

#ifndef __VGA_H__
#define __VGA_H__

#include "../../common/colors.h"

// Cor global - altera a cor de tudo que for impresso depois
void vga_set_color(unsigned char attr);
unsigned char vga_get_color(void);

// Limpa a tela (usa a cor atual)
void vga_clear(void);

// Escrita básica (usa a cor atual)
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_putint(long x);
void vga_puthex(unsigned long x);

// Escrita com cor específica (não altera a cor global)
void vga_putchar_color(char c, unsigned char attr);
void vga_puts_color(const char *s, unsigned char attr);

#endif
