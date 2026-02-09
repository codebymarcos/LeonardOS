// LeonardOS - PIT (Programmable Interval Timer) 8253/8254
// Canal 0 configurado a 100 Hz para tick do sistema
//
// PIT base frequency: 1,193,182 Hz
// Divisor para 100 Hz: 1193182 / 100 = 11932 (0x2E9C)

#include "pit.h"
#include "../../common/io.h"
#include "../../cpu/isr.h"
#include "../../drivers/pic/pic.h"
#include "../../drivers/vga/vga.h"
#include "../../common/colors.h"

// ============================================================
// PIT I/O Ports
// ============================================================
#define PIT_CHANNEL0    0x40    // Channel 0 data port
#define PIT_CHANNEL1    0x41    // Channel 1 data port
#define PIT_CHANNEL2    0x42    // Channel 2 data port
#define PIT_COMMAND     0x43    // Mode/command register

// ============================================================
// PIT Command bits
// ============================================================
// Bits 7:6 = Channel select (00 = channel 0)
// Bits 5:4 = Access mode (11 = lo/hi byte)
// Bits 3:1 = Operating mode (010 = mode 2, rate generator)
// Bit  0   = BCD/Binary (0 = 16-bit binary)
#define PIT_CMD_CH0_RATE  0x34  // Channel 0, lo/hi, mode 2 (rate generator), binary

// ============================================================
// Constantes
// ============================================================
#define PIT_BASE_FREQ   1193182
#define PIT_DIVISOR     (PIT_BASE_FREQ / PIT_HZ)  // 11932 para 100Hz

// ============================================================
// Estado do timer
// ============================================================
static volatile uint32_t tick_count = 0;

// ============================================================
// IRQ0 handler — incrementa tick counter
// ============================================================
static void pit_irq_handler(struct isr_frame *frame) {
    (void)frame;
    tick_count++;
}

// ============================================================
// pit_get_ticks — retorna ticks desde o boot
// ============================================================
uint32_t pit_get_ticks(void) {
    return tick_count;
}

// ============================================================
// pit_get_ms — retorna milissegundos desde o boot
// ============================================================
uint32_t pit_get_ms(void) {
    return tick_count * PIT_MS_PER_TICK;
}

// ============================================================
// pit_sleep_ms — delay bloqueante baseado em ticks
// Muito mais preciso que busy-wait loops
// ============================================================
void pit_sleep_ms(uint32_t ms) {
    uint32_t ticks_to_wait = ms / PIT_MS_PER_TICK;
    if (ticks_to_wait == 0 && ms > 0) ticks_to_wait = 1;

    uint32_t start = tick_count;
    while ((tick_count - start) < ticks_to_wait) {
        // Halt CPU até próxima interrupção — economiza energia
        asm volatile("hlt");
    }
}

// ============================================================
// pit_init — configura PIT canal 0 a 100Hz e registra IRQ0
// ============================================================
void pit_init(void) {
    tick_count = 0;

    // Configura canal 0: mode 2 (rate generator), lo/hi access
    outb(PIT_COMMAND, PIT_CMD_CH0_RATE);

    // Envia divisor (lo byte primeiro, depois hi byte)
    outb(PIT_CHANNEL0, (uint8_t)(PIT_DIVISOR & 0xFF));        // lo byte
    outb(PIT_CHANNEL0, (uint8_t)((PIT_DIVISOR >> 8) & 0xFF)); // hi byte

    // Registra handler para IRQ0 (INT 32)
    isr_register_handler(IRQ_TO_INT(IRQ_TIMER), pit_irq_handler);

    // Habilita IRQ0 no PIC
    pic_unmask_irq(IRQ_TIMER);

    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("PIT: timer a ", THEME_BOOT);
    vga_putint(PIT_HZ);
    vga_puts_color("Hz (", THEME_BOOT);
    vga_putint(PIT_MS_PER_TICK);
    vga_puts_color("ms/tick)\n", THEME_BOOT);
}
