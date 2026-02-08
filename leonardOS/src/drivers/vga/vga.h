// VGA Driver - Interface pública

#ifndef __VGA_H__
#define __VGA_H__

void vga_clear(void);
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_putint(long x);
void vga_puthex(unsigned long x);

#endif
