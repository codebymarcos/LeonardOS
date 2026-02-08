// LeonardOS - Driver Teclado PS/2
// Baseado em IRQ1 (interrupção de hardware)

#include "keyboard.h"
#include "../vga/vga.h"
#include "../pic/pic.h"
#include "../../cpu/isr.h"
#include "../../common/io.h"

// Portas I/O PS/2
#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

// Buffer circular de teclado
#define KBD_BUFFER_SIZE 256
static volatile unsigned char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

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

// Adiciona caractere ao buffer
static void kbd_enqueue(char c) {
    if (c == 0) return;
    
    int next_head = (kbd_head + 1) % KBD_BUFFER_SIZE;
    if (next_head != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next_head;
    }
}

// Handler de interrupção IRQ1 - chamado automaticamente pelo PIC
static void kbd_irq_handler(struct isr_frame *frame) {
    (void)frame;

    unsigned char scancode = inb(KBD_DATA_PORT);

    // Ignora key release (bit 7 = 1)
    if (scancode & 0x80) {
        return;
    }

    if (scancode < 128) {
        char c = scancode_map[scancode];
        kbd_enqueue(c);
    }
}

// Inicializa o driver de teclado (registra IRQ1)
void kbd_init(void) {
    // Registra handler para IRQ1 (INT 33)
    isr_register_handler(IRQ_TO_INT(IRQ_KEYBOARD), kbd_irq_handler);

    // Habilita IRQ1 no PIC
    pic_unmask_irq(IRQ_KEYBOARD);
}

// Lê um caractere do buffer (bloqueia até haver dados)
char kbd_getchar(void) {
    while (kbd_head == kbd_tail) {
        // Espera interrupção (CPU descansa até a próxima IRQ)
        asm volatile("hlt");
    }
    
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return c;
}

// Verifica se há caractere pronto
int kbd_has_char(void) {
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
