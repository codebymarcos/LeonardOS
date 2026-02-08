// LeonardOS - ISR (Interrupt Service Routines)
// Dispatcher central de interrupções

#include "isr.h"
#include "idt.h"
#include "gdt.h"
#include "../common/io.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// Tabela de handlers registrados
static isr_handler_t isr_handlers[IDT_NUM_ENTRIES];

// Declarações dos stubs assembly (definidos em isr_stub.s)
// Exceções da CPU (0-31)
extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_2(void);
extern void isr_stub_3(void);
extern void isr_stub_4(void);
extern void isr_stub_5(void);
extern void isr_stub_6(void);
extern void isr_stub_7(void);
extern void isr_stub_8(void);
extern void isr_stub_9(void);
extern void isr_stub_10(void);
extern void isr_stub_11(void);
extern void isr_stub_12(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
extern void isr_stub_15(void);
extern void isr_stub_16(void);
extern void isr_stub_17(void);
extern void isr_stub_18(void);
extern void isr_stub_19(void);
extern void isr_stub_20(void);
extern void isr_stub_21(void);
extern void isr_stub_22(void);
extern void isr_stub_23(void);
extern void isr_stub_24(void);
extern void isr_stub_25(void);
extern void isr_stub_26(void);
extern void isr_stub_27(void);
extern void isr_stub_28(void);
extern void isr_stub_29(void);
extern void isr_stub_30(void);
extern void isr_stub_31(void);

// IRQs (32-47)
extern void irq_stub_0(void);
extern void irq_stub_1(void);
extern void irq_stub_2(void);
extern void irq_stub_3(void);
extern void irq_stub_4(void);
extern void irq_stub_5(void);
extern void irq_stub_6(void);
extern void irq_stub_7(void);
extern void irq_stub_8(void);
extern void irq_stub_9(void);
extern void irq_stub_10(void);
extern void irq_stub_11(void);
extern void irq_stub_12(void);
extern void irq_stub_13(void);
extern void irq_stub_14(void);
extern void irq_stub_15(void);

// Nomes das exceções da CPU
static const char *exception_names[] = {
    "Division By Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization",
    "Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved",
    "Hypervisor Injection",
    "VMM Communication",
    "Security Exception",
    "Reserved"
};

// Registra um handler
void isr_register_handler(uint8_t int_no, isr_handler_t handler) {
    isr_handlers[int_no] = handler;
}

// Dispatcher central - chamado pelo stub assembly
void isr_dispatcher(struct isr_frame *frame) {
    // Se há um handler registrado, chama ele
    if (isr_handlers[frame->int_no] != NULL) {
        isr_handlers[frame->int_no](frame);
    } else if (frame->int_no < 32) {
        // Exceção sem handler -> panic
        vga_puts_color("\n!!! KERNEL PANIC !!!\n", THEME_BOOT_FAIL);
        vga_puts_color("Excecao: ", THEME_ERROR);
        vga_puts_color(exception_names[frame->int_no], THEME_WARNING);
        vga_puts_color(" (INT ", THEME_ERROR);
        vga_putint(frame->int_no);
        vga_puts_color(")\n", THEME_ERROR);
        vga_puts_color("Error code: ", THEME_LABEL);
        vga_puthex(frame->err_code);
        vga_puts_color("\nEIP: ", THEME_LABEL);
        vga_puthex(frame->eip);
        vga_puts_color("  CS: ", THEME_LABEL);
        vga_puthex(frame->cs);
        vga_puts("\n");
        // Halt
        asm volatile("cli; hlt");
        while (1);
    }

    // Para IRQs (32-47): envia EOI ao PIC
    if (frame->int_no >= 32 && frame->int_no < 48) {
        // IRQ do PIC slave (8-15 -> INT 40-47)
        if (frame->int_no >= 40) {
            outb(0xA0, 0x20);  // EOI ao slave
        }
        outb(0x20, 0x20);  // EOI ao master
    }
}

// Inicializa ISRs e registra na IDT
void isr_init(void) {
    // Zera handlers
    for (int i = 0; i < IDT_NUM_ENTRIES; i++) {
        isr_handlers[i] = NULL;
    }

    // Zera a IDT antes de preencher
    idt_init();

    // Registra exceções da CPU (0-31)
    idt_set_entry(0,  (uint32_t)isr_stub_0,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(1,  (uint32_t)isr_stub_1,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(2,  (uint32_t)isr_stub_2,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(3,  (uint32_t)isr_stub_3,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(4,  (uint32_t)isr_stub_4,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(5,  (uint32_t)isr_stub_5,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(6,  (uint32_t)isr_stub_6,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(7,  (uint32_t)isr_stub_7,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(8,  (uint32_t)isr_stub_8,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(9,  (uint32_t)isr_stub_9,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(10, (uint32_t)isr_stub_10, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(11, (uint32_t)isr_stub_11, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(12, (uint32_t)isr_stub_12, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(13, (uint32_t)isr_stub_13, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(14, (uint32_t)isr_stub_14, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(15, (uint32_t)isr_stub_15, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(16, (uint32_t)isr_stub_16, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(17, (uint32_t)isr_stub_17, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(18, (uint32_t)isr_stub_18, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(19, (uint32_t)isr_stub_19, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(20, (uint32_t)isr_stub_20, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(21, (uint32_t)isr_stub_21, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(22, (uint32_t)isr_stub_22, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(23, (uint32_t)isr_stub_23, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(24, (uint32_t)isr_stub_24, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(25, (uint32_t)isr_stub_25, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(26, (uint32_t)isr_stub_26, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(27, (uint32_t)isr_stub_27, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(28, (uint32_t)isr_stub_28, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(29, (uint32_t)isr_stub_29, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(30, (uint32_t)isr_stub_30, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(31, (uint32_t)isr_stub_31, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);

    // Registra IRQs (32-47)
    idt_set_entry(32, (uint32_t)irq_stub_0,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(33, (uint32_t)irq_stub_1,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(34, (uint32_t)irq_stub_2,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(35, (uint32_t)irq_stub_3,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(36, (uint32_t)irq_stub_4,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(37, (uint32_t)irq_stub_5,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(38, (uint32_t)irq_stub_6,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(39, (uint32_t)irq_stub_7,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(40, (uint32_t)irq_stub_8,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(41, (uint32_t)irq_stub_9,  GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(42, (uint32_t)irq_stub_10, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(43, (uint32_t)irq_stub_11, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(44, (uint32_t)irq_stub_12, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(45, (uint32_t)irq_stub_13, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(46, (uint32_t)irq_stub_14, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);
    idt_set_entry(47, (uint32_t)irq_stub_15, GDT_KERNEL_CODE_SEG, IDT_GATE_KERNEL);

    // Recarrega a IDT com as entradas preenchidas
    idt_load();
}
