// LeonardOS - PIC (Programmable Interrupt Controller) 8259
// Remapeia IRQs para não conflitar com exceções da CPU

#include "pic.h"
#include "../../common/io.h"

// Portas do PIC
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

// ICW (Initialization Command Words)
#define ICW1_INIT    0x10   // Inicialização
#define ICW1_ICW4    0x01   // ICW4 será enviado
#define ICW4_8086    0x01   // Modo 8086

// Offset das IRQs (para onde remapear)
#define PIC1_OFFSET  32     // IRQ 0-7  -> INT 32-39
#define PIC2_OFFSET  40     // IRQ 8-15 -> INT 40-47

// Inicializa e remapeia o PIC
void pic_init(void) {
    // Salva as máscaras atuais
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    // ICW1: Inicia sequência de inicialização (cascade + ICW4)
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // ICW2: Offset do vetor de interrupções
    outb(PIC1_DATA, PIC1_OFFSET);  // Master: IRQ 0-7  -> INT 32-39
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);  // Slave:  IRQ 8-15 -> INT 40-47
    io_wait();

    // ICW3: Configuração de cascade
    outb(PIC1_DATA, 0x04);  // Master: slave no IRQ2 (bit 2)
    io_wait();
    outb(PIC2_DATA, 0x02);  // Slave: cascade identity 2
    io_wait();

    // ICW4: Modo 8086
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Mascara todas as IRQs inicialmente (exceto cascade IRQ2)
    (void)mask1;
    (void)mask2;
    outb(PIC1_DATA, 0xFB);  // Tudo mascarado exceto IRQ2 (cascade)
    outb(PIC2_DATA, 0xFF);  // Tudo mascarado no slave
}

// Envia End-Of-Interrupt
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);  // EOI ao slave
    }
    outb(PIC1_COMMAND, 0x20);  // EOI ao master
}

// Habilita (unmask) uma IRQ
void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

// Desabilita (mask) uma IRQ
void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (1 << irq);
    outb(port, value);
}
