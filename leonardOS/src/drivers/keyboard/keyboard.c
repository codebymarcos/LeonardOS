// LeonardOS - Driver Teclado PS/2
// Lê de uma fila simples

#include "keyboard.h"
#include "../vga/vga.h"

// Portas I/O PS/2
#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

// Buffer circular de teclado
#define KBD_BUFFER_SIZE 256
static unsigned char kbd_buffer[KBD_BUFFER_SIZE];
static int kbd_head = 0;
static int kbd_tail = 0;

// Mapa de scancodes para ASCII (US layout simplificado)
static const char scancode_map[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
    'b',  'n', 'm', ',', '.', '/',  0,  '*', 0,  ' ', 0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2',  '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// Lê status da porta do teclado
static unsigned char kbd_read_status(void) {
    unsigned char status;
    asm volatile("inb %1, %0" : "=a" (status) : "Nd" (KBD_STATUS_PORT));
    return status;
}

// Lê dados do teclado
static unsigned char kbd_read_data(void) {
    unsigned char data;
    asm volatile("inb %1, %0" : "=a" (data) : "Nd" (KBD_DATA_PORT));
    return data;
}

// Adiciona caractere ao buffer
static void kbd_enqueue(char c) {
    if (c == 0) return;
    
    int next_head = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next_head != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next_head;
    }
}

// Handler de interrupção
void kbd_interrupt_handler(void) {
    unsigned char status = kbd_read_status();
    
    if ((status & 0x01) == 0) {
        return;
    }
    
    unsigned char scancode = kbd_read_data();
    
    if (scancode & 0x80) {
        return;
    }
    
    if (scancode < 128) {
        char c = scancode_map[scancode];
        kbd_enqueue(c);
    }
}

// Lê um caractere do buffer
char kbd_getchar(void) {
    while (kbd_head == kbd_tail) {
        kbd_interrupt_handler();
    }
    
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

// Verifica se há caractere pronto
int kbd_has_char(void) {
    kbd_interrupt_handler();
    return kbd_head != kbd_tail;
}

// Lê uma linha (até newline) com echo
void kbd_read_line(char *buf, int maxlen) {
    int i = 0;
    while (i < maxlen - 1) {
        char c = kbd_getchar();
        if (c == '\n' || c == '\r') {
            break;
        }
        if (c == '\b') {
            if (i > 0) {
                i--;
                vga_putchar('\b');
            }
        } else if (c >= 32 && c < 127) {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
    buf[i] = 0;
}
