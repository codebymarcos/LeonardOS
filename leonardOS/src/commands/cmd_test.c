// LeonardOS - Comando: test
// Teste automatizado e preciso de todos os subsistemas do kernel
// Coleta informações de debug e valida integridade do sistema

#include "cmd_test.h"
#include "commands.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/types.h"
#include "../common/io.h"
#include "../cpu/gdt.h"
#include "../cpu/idt.h"
#include "../cpu/isr.h"
#include "../drivers/pic/pic.h"
#include "../drivers/keyboard/keyboard.h"
#include "../memory/pmm.h"

// ============================================================
// Contadores de resultado
// ============================================================
static int tests_passed;
static int tests_failed;
static int tests_total;

// ============================================================
// Helpers de output
// ============================================================
static void test_header(const char *section) {
    vga_puts_color("\n── ", THEME_BORDER);
    vga_puts_color(section, THEME_TITLE);
    vga_puts_color(" ", THEME_BORDER);
    // Preenche com ─ até completar
    int len = 0;
    const char *p = section;
    while (*p++) len++;
    for (int i = len + 4; i < 50; i++) {
        vga_puts_color("─", THEME_BORDER);
    }
    vga_putchar('\n');
}

static void test_result(const char *name, int passed, const char *detail) {
    tests_total++;
    if (passed) {
        tests_passed++;
        vga_puts_color("  [OK]   ", THEME_BOOT_OK);
    } else {
        tests_failed++;
        vga_puts_color("  [FAIL] ", THEME_BOOT_FAIL);
    }
    vga_puts_color(name, THEME_DEFAULT);
    if (detail) {
        vga_puts_color(" (", THEME_DIM);
        vga_puts_color(detail, THEME_DIM);
        vga_puts_color(")", THEME_DIM);
    }
    vga_putchar('\n');
}

static void test_info(const char *label, const char *value) {
    vga_puts_color("  [INFO] ", THEME_INFO);
    vga_puts_color(label, THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_puts_color(value, THEME_VALUE);
    vga_putchar('\n');
}

static void test_info_hex(const char *label, uint32_t value) {
    vga_puts_color("  [INFO] ", THEME_INFO);
    vga_puts_color(label, THEME_LABEL);
    vga_puts_color(": 0x", THEME_DIM);
    vga_puthex(value);
    vga_putchar('\n');
}

static void test_info_int(const char *label, int value) {
    vga_puts_color("  [INFO] ", THEME_INFO);
    vga_puts_color(label, THEME_LABEL);
    vga_puts_color(": ", THEME_DIM);
    vga_putint(value);
    vga_putchar('\n');
}

// ============================================================
// 1. Teste de CPU - Registradores e modos
// ============================================================
static void test_cpu(void) {
    test_header("CPU / Registradores");

    // Testa CR0 (bit 0 = Protected Mode ativo)
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    test_result("CR0 Protected Mode (PE bit)", cr0 & 0x1, NULL);
    test_info_hex("CR0", cr0);

    // Testa se paging está desativado (bit 31 de CR0)
    int paging = (cr0 >> 31) & 1;
    test_result("CR0 Paging desativado", !paging, paging ? "ATIVO" : "desativado");

    // Lê EFLAGS
    uint32_t eflags;
    asm volatile("pushfl; pop %0" : "=r"(eflags));
    test_info_hex("EFLAGS", eflags);

    // Testa se interrupts estão habilitadas (IF flag, bit 9)
    int if_set = (eflags >> 9) & 1;
    test_result("EFLAGS Interrupts habilitadas (IF)", if_set, NULL);

    // Lê CS para verificar seletor
    uint16_t cs;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    test_result("CS = 0x08 (Kernel Code)", cs == 0x08, NULL);
    test_info_hex("CS", cs);

    // Lê DS
    uint16_t ds;
    asm volatile("mov %%ds, %0" : "=r"(ds));
    test_result("DS = 0x10 (Kernel Data)", ds == 0x10, NULL);
    test_info_hex("DS", ds);

    // Lê SS
    uint16_t ss;
    asm volatile("mov %%ss, %0" : "=r"(ss));
    test_result("SS = 0x10 (Kernel Data)", ss == 0x10, NULL);
    test_info_hex("SS", ss);

    // Lê ESP (stack pointer)
    uint32_t esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    test_info_hex("ESP (stack pointer)", esp);
    // ESP válido: não-nulo, fora da zona IVT/BDA (< 0x1000), alinhado a 4 bytes
    test_result("ESP valido (> 0x1000, alinhado)", esp > 0x1000 && (esp % 4) == 0, NULL);
}

// ============================================================
// 2. Teste da GDT
// ============================================================
static void test_gdt(void) {
    test_header("GDT (Global Descriptor Table)");

    // Lê o ponteiro da GDT atual via SGDT
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) gdtr;

    asm volatile("sgdt %0" : "=m"(gdtr));

    test_info_hex("GDTR Base", gdtr.base);
    test_info_int("GDTR Limit", gdtr.limit);

    // GDT com 3 entradas = 3 * 8 = 24 bytes, limit = 23
    int expected_limit = (GDT_NUM_ENTRIES * 8) - 1;
    test_result("GDT limit correto", gdtr.limit == expected_limit, NULL);
    test_result("GDT base != 0", gdtr.base != 0, NULL);

    // Lê as entradas da GDT diretamente
    struct gdt_entry *entries = (struct gdt_entry *)(uintptr_t)gdtr.base;

    // Entry 0: Null segment (tudo zero)
    int null_ok = (entries[0].access == 0 && entries[0].limit_low == 0 &&
                   entries[0].base_low == 0);
    test_result("GDT[0] Null segment", null_ok, NULL);

    // Entry 1: Kernel Code (access=0x9A ou 0x9B com Accessed bit)
    // A CPU seta o bit 0 (Accessed) automaticamente ao usar o segmento
    int code_access_ok = (entries[1].access == 0x9A || entries[1].access == 0x9B);
    test_result("GDT[1] Code access=0x9A/0x9B",
                code_access_ok, NULL);
    test_result("GDT[1] Code gran=0xCF",
                entries[1].granularity == 0xCF, NULL);
    test_info_hex("GDT[1] access", entries[1].access);
    test_info_hex("GDT[1] granularity", entries[1].granularity);

    // Entry 2: Kernel Data (access=0x92 ou 0x93 com Accessed bit)
    int data_access_ok = (entries[2].access == 0x92 || entries[2].access == 0x93);
    test_result("GDT[2] Data access=0x92/0x93",
                data_access_ok, NULL);
    test_result("GDT[2] Data gran=0xCF",
                entries[2].granularity == 0xCF, NULL);
    test_info_hex("GDT[2] access", entries[2].access);
    test_info_hex("GDT[2] granularity", entries[2].granularity);
}

// ============================================================
// 3. Teste da IDT
// ============================================================
static void test_idt(void) {
    test_header("IDT (Interrupt Descriptor Table)");

    // Lê o ponteiro da IDT via SIDT
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) idtr;

    asm volatile("sidt %0" : "=m"(idtr));

    test_info_hex("IDTR Base", idtr.base);
    test_info_int("IDTR Limit", idtr.limit);

    // IDT com 256 entradas = 256 * 8 = 2048 bytes, limit = 2047
    int expected_limit = (IDT_NUM_ENTRIES * 8) - 1;
    test_result("IDT limit correto (2047)", idtr.limit == expected_limit, NULL);
    test_result("IDT base != 0", idtr.base != 0, NULL);

    // Verifica entradas críticas
    struct idt_entry *entries = (struct idt_entry *)(uintptr_t)idtr.base;

    // ISR 0 (Division by Zero) deve estar presente
    int isr0_present = (entries[0].flags & IDT_FLAG_PRESENT) != 0;
    test_result("IDT[0]  Division By Zero presente", isr0_present, NULL);

    // ISR 13 (GPF) deve estar presente
    int isr13_present = (entries[13].flags & IDT_FLAG_PRESENT) != 0;
    test_result("IDT[13] General Protection presente", isr13_present, NULL);

    // ISR 14 (Page Fault) deve estar presente
    int isr14_present = (entries[14].flags & IDT_FLAG_PRESENT) != 0;
    test_result("IDT[14] Page Fault presente", isr14_present, NULL);

    // IRQ1 (Keyboard = INT 33) deve estar presente
    int irq1_present = (entries[33].flags & IDT_FLAG_PRESENT) != 0;
    test_result("IDT[33] IRQ1 Keyboard presente", irq1_present, NULL);

    // Verifica seletor das entradas (deve ser 0x08 = kernel code)
    test_result("IDT[0] selector=0x08",
                entries[0].selector == GDT_KERNEL_CODE_SEG, NULL);
    test_result("IDT[33] selector=0x08",
                entries[33].selector == GDT_KERNEL_CODE_SEG, NULL);

    // Conta quantas entradas estão presentes
    int present_count = 0;
    for (int i = 0; i < IDT_NUM_ENTRIES; i++) {
        if (entries[i].flags & IDT_FLAG_PRESENT) {
            present_count++;
        }
    }
    test_info_int("Entradas presentes", present_count);
    // Esperamos 32 exceções + 16 IRQs = 48
    test_result("48 entradas preenchidas (32 ISR + 16 IRQ)",
                present_count == 48, NULL);

    // Dump dos handlers das primeiras entradas e IRQ1
    uint32_t handler0 = entries[0].base_low | ((uint32_t)entries[0].base_high << 16);
    uint32_t handler33 = entries[33].base_low | ((uint32_t)entries[33].base_high << 16);
    test_info_hex("Handler ISR 0  (Div0)", handler0);
    test_info_hex("Handler INT 33 (KBD)", handler33);
    test_result("Handler ISR 0 != 0", handler0 != 0, NULL);
    test_result("Handler INT 33 != 0", handler33 != 0, NULL);
}

// ============================================================
// 4. Teste do PIC
// ============================================================
static void test_pic(void) {
    test_header("PIC (8259 Interrupt Controller)");

    // Lê IMR (Interrupt Mask Register) do master e slave
    uint8_t master_mask = inb(0x21);
    uint8_t slave_mask  = inb(0xA1);

    test_info_hex("Master PIC mask (0x21)", master_mask);
    test_info_hex("Slave PIC mask  (0xA1)", slave_mask);

    // IRQ1 (keyboard) deve estar unmasked (bit 1 = 0)
    int kbd_unmasked = !(master_mask & (1 << 1));
    test_result("IRQ1 (Keyboard) unmasked", kbd_unmasked, NULL);

    // IRQ2 (cascade) deve estar unmasked (bit 2 = 0)
    int cascade_unmasked = !(master_mask & (1 << 2));
    test_result("IRQ2 (Cascade) unmasked", cascade_unmasked, NULL);

    // IRQ0 (timer) - reporta status
    int timer_masked = (master_mask & (1 << 0));
    test_info(timer_masked ? "IRQ0 (Timer) masked" : "IRQ0 (Timer) unmasked",
              timer_masked ? "sem PIT configurado" : "ativo");

    // Lê ISR (In-Service Register) do master
    outb(0x20, 0x0B);  // OCW3: read ISR
    uint8_t master_isr = inb(0x20);
    test_info_hex("Master ISR (in-service)", master_isr);

    // Lê IRR (Interrupt Request Register) do master
    outb(0x20, 0x0A);  // OCW3: read IRR
    uint8_t master_irr = inb(0x20);
    test_info_hex("Master IRR (pending)", master_irr);

    // Reporta IRQs ativas individualmente
    for (int i = 0; i < 8; i++) {
        if (!(master_mask & (1 << i))) {
            vga_puts_color("  [INFO] ", THEME_INFO);
            vga_puts_color("IRQ", THEME_LABEL);
            vga_putint(i);
            vga_puts_color(": habilitada\n", THEME_VALUE);
        }
    }
}

// ============================================================
// 5. Teste de VGA
// ============================================================
static void test_vga(void) {
    test_header("VGA (Video Graphics Array)");

    // Verifica que o buffer VGA está acessível
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;

    // Salva o valor original
    uint16_t original = vga[0];

    // Escreve valor de teste
    vga[0] = 0x0741;  // 'A' com atributo 0x07 (branco/preto)
    uint16_t readback = vga[0];

    // Restaura
    vga[0] = original;

    test_result("VGA buffer acessivel (0xB8000)", readback == 0x0741, NULL);
    test_info_hex("VGA buffer readback", readback);

    // Testa cursor (lê posição do cursor do CRTC)
    outb(0x3D4, 14);  // High byte
    uint8_t cursor_hi = inb(0x3D5);
    outb(0x3D4, 15);  // Low byte
    uint8_t cursor_lo = inb(0x3D5);
    uint16_t cursor_pos = ((uint16_t)cursor_hi << 8) | cursor_lo;

    int cursor_row = cursor_pos / 80;
    int cursor_col = cursor_pos % 80;

    test_info_int("Cursor posicao", cursor_pos);
    test_info_int("Cursor linha", cursor_row);
    test_info_int("Cursor coluna", cursor_col);
    test_result("Cursor em faixa valida (<2000)", cursor_pos < 2000, NULL);

    // Testa cor atual
    uint8_t current_color = vga_get_color();
    test_info_hex("Cor atual (attr)", current_color);
    test_result("Cor != 0 (configurada)", current_color != 0, NULL);
}

// ============================================================
// 6. Teste de Memória básica
// ============================================================
static void test_memory(void) {
    test_header("Memoria");

    // Testa escrita/leitura em stack
    volatile uint32_t stack_test = 0xDEADBEEF;
    test_result("Stack R/W (0xDEADBEEF)", stack_test == 0xDEADBEEF, NULL);

    // Testa BSS (variável estática zerada)
    static uint32_t bss_test;
    test_result("BSS zerado", bss_test == 0, NULL);

    // Testa escrita em BSS
    bss_test = 0xCAFEBABE;
    test_result("BSS R/W (0xCAFEBABE)", bss_test == 0xCAFEBABE, NULL);
    bss_test = 0;  // restaura

    // Testa alinhamento de stack
    uint32_t esp;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    test_result("Stack alinhado (4 bytes)", (esp % 4) == 0, NULL);
    test_info_hex("ESP atual", esp);

    // Testa acesso ao kernel space (região > 1MB)
    volatile uint8_t *kernel_mem = (volatile uint8_t *)0x100000;
    // Apenas leitura para verificar acesso sem fault
    uint8_t val = *kernel_mem;
    (void)val;
    test_result("Kernel space acessivel (0x100000)", 1, NULL);
}

// ============================================================
// 7. Teste do Keyboard (sem bloquear)
// ============================================================
static void test_keyboard(void) {
    test_header("Keyboard (PS/2)");

    // Lê status do controller 8042
    uint8_t status = inb(0x64);
    test_info_hex("8042 Status Register", status);

    // Bit 0: Output buffer cheio (dado disponível)
    // Bit 1: Input buffer cheio (controlador ocupado)
    // Bit 2: System flag (POST passou)
    // Bit 4: Keyboard habilitado
    int sys_flag = (status >> 2) & 1;
    int input_busy = (status >> 1) & 1;

    test_result("8042 System flag (POST OK)", sys_flag, NULL);
    test_result("8042 Input buffer livre", !input_busy, NULL);

    // Verifica se IRQ1 está registrada na IDT
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) idtr;
    asm volatile("sidt %0" : "=m"(idtr));
    struct idt_entry *entries = (struct idt_entry *)(uintptr_t)idtr.base;
    int kbd_handler_set = (entries[33].flags & IDT_FLAG_PRESENT) != 0;
    test_result("IRQ1 handler registrado (INT 33)", kbd_handler_set, NULL);

    // Verifica buffer do keyboard
    int has_char = kbd_has_char();
    test_info(has_char ? "Buffer do keyboard" : "Buffer do keyboard", 
              has_char ? "tem dados" : "vazio");
}

// ============================================================
// 8. Teste de I/O Ports
// ============================================================
static void test_io_ports(void) {
    test_header("I/O Ports");

    // CMOS/RTC - Porta 0x70/0x71
    // Lê o segundo atual do RTC para verificar I/O
    outb(0x70, 0x00);  // Registrador 0 = segundos
    uint8_t rtc_sec = inb(0x71);
    test_info_hex("RTC segundos (BCD)", rtc_sec);
    test_result("RTC acessivel (porta 0x70/71)", rtc_sec <= 0x59, NULL);

    // Lê Status Register A do RTC
    outb(0x70, 0x0A);
    uint8_t rtc_status_a = inb(0x71);
    test_info_hex("RTC Status A", rtc_status_a);

    // PIT (8253/8254) - Porta 0x40-0x43
    // Lê counter 0 (timer)
    outb(0x43, 0x00);  // Latch counter 0
    uint8_t pit_lo = inb(0x40);
    uint8_t pit_hi = inb(0x40);
    uint16_t pit_count = ((uint16_t)pit_hi << 8) | pit_lo;
    test_info_int("PIT Counter 0", pit_count);
    test_result("PIT acessivel (porta 0x40)", 1, NULL);

    // Porta serial COM1 (0x3F8) - Line Status Register
    uint8_t com1_lsr = inb(0x3FD);
    test_info_hex("COM1 Line Status", com1_lsr);
}

// ============================================================
// 9. Teste do PMM (Physical Memory Manager)
// ============================================================
static void test_pmm(void) {
    test_header("PMM (Physical Memory Manager)");

    // Verifica que PMM foi inicializado com estatísticas válidas
    struct pmm_stats stats = pmm_get_stats();

    test_info_int("RAM total (KB)", stats.total_memory_kb);
    test_info_int("Frames totais", stats.total_frames);
    test_info_int("Frames usados", stats.used_frames);
    test_info_int("Frames livres", stats.free_frames);
    test_info_int("Frames do kernel", stats.kernel_frames);

    // Verifica que detectou memória
    test_result("RAM detectada (> 0)", stats.total_memory_kb > 0, NULL);
    test_result("Frames totais > 0", stats.total_frames > 0, NULL);
    test_result("Frames livres > 0", stats.free_frames > 0, NULL);
    test_result("Kernel usa frames", stats.kernel_frames > 0, NULL);
    test_result("Consistencia: total = used + free",
                stats.total_frames == stats.used_frames + stats.free_frames, NULL);

    // Teste de alloc/free
    uint32_t frame1 = pmm_alloc_frame();
    test_result("pmm_alloc_frame() != 0", frame1 != 0, NULL);
    test_info_hex("Frame alocado", frame1);
    test_result("Frame alocado alinhado (4KB)", frame1 % PMM_FRAME_SIZE == 0, NULL);
    test_result("Frame marcado como usado", pmm_is_frame_used(frame1), NULL);

    // Aloca segundo frame (deve ser diferente)
    uint32_t frame2 = pmm_alloc_frame();
    test_result("Segundo frame != primeiro", frame2 != frame1, NULL);
    test_result("Segundo frame != 0", frame2 != 0, NULL);

    // Verifica stats após alocação
    struct pmm_stats stats_after = pmm_get_stats();
    test_result("used_frames aumentou +2",
                stats_after.used_frames == stats.used_frames + 2, NULL);

    // Free frame 1
    pmm_free_frame(frame1);
    test_result("Frame 1 liberado (nao usado)", !pmm_is_frame_used(frame1), NULL);

    // Free frame 2
    pmm_free_frame(frame2);
    test_result("Frame 2 liberado (nao usado)", !pmm_is_frame_used(frame2), NULL);

    // Stats devem voltar ao original
    struct pmm_stats stats_restored = pmm_get_stats();
    test_result("Stats restaurados apos free",
                stats_restored.used_frames == stats.used_frames, NULL);

    // Double-free: liberar frame já livre não deve crashar
    pmm_free_frame(frame1);
    struct pmm_stats stats_dbl = pmm_get_stats();
    test_result("Double-free seguro (stats inalterados)",
                stats_dbl.used_frames == stats.used_frames, NULL);

    // Frame 0 (NULL) nunca deve ser alocável
    test_result("Frame 0x0 marcado como usado", pmm_is_frame_used(0), NULL);

    // Kernel region deve estar protegida
    test_result("Kernel (0x100000) protegido", pmm_is_frame_used(0x100000), NULL);
}

// ============================================================
// 10. Teste do sistema de Comandos
// ============================================================
static void test_commands(void) {
    test_header("Sistema de Comandos");

    int count = commands_get_count();
    test_info_int("Comandos registrados", count);
    test_result("Pelo menos 4 comandos", count >= 4, NULL);

    // Verifica se cada comando base existe
    const char *expected[] = {"help", "clear", "sysinfo", "halt", "test", "mem"};
    int num_expected = 6;

    for (int i = 0; i < num_expected; i++) {
        const command_t *cmd = commands_find(expected[i]);
        int found = (cmd != NULL);
        vga_puts_color("  ", THEME_DEFAULT);
        if (found) {
            vga_puts_color("[OK]   ", THEME_BOOT_OK);
        } else {
            vga_puts_color("[FAIL] ", THEME_BOOT_FAIL);
        }
        vga_puts_color("Comando '", THEME_DEFAULT);
        vga_puts_color(expected[i], THEME_INFO);
        vga_puts_color("' registrado\n", THEME_DEFAULT);
        tests_total++;
        if (found) tests_passed++;
        else tests_failed++;
    }
}

// ============================================================
// Ponto de entrada do comando test
// ============================================================
void cmd_test(const char *args) {
    (void)args;

    tests_passed = 0;
    tests_failed = 0;
    tests_total = 0;

    vga_puts("\n");
    vga_puts_color("╔══════════════════════════════════════════════════╗\n", THEME_BORDER);
    vga_puts_color("║", THEME_BORDER);
    vga_puts_color("    LeonardOS - Teste Automatizado do Kernel     ", THEME_TITLE);
    vga_puts_color("║\n", THEME_BORDER);
    vga_puts_color("╚══════════════════════════════════════════════════╝\n", THEME_BORDER);

    // Executa todos os testes
    test_cpu();
    test_gdt();
    test_idt();
    test_pic();
    test_vga();
    test_memory();
    test_keyboard();
    test_io_ports();
    test_pmm();
    test_commands();

    // Resumo final
    vga_puts_color("\n══════════════════════════════════════════════════\n", THEME_BORDER);
    vga_puts_color("  RESULTADO: ", THEME_TITLE);

    vga_putint(tests_total);
    vga_puts_color(" testes, ", THEME_DEFAULT);

    vga_puts_color("", THEME_BOOT_OK);
    vga_set_color(THEME_BOOT_OK);
    vga_putint(tests_passed);
    vga_puts(" OK");

    vga_puts_color(", ", THEME_DEFAULT);

    if (tests_failed > 0) {
        vga_set_color(THEME_BOOT_FAIL);
        vga_putint(tests_failed);
        vga_puts(" FALHOU");
    } else {
        vga_set_color(THEME_BOOT_OK);
        vga_putint(tests_failed);
        vga_puts(" FALHOU");
    }

    vga_set_color(THEME_DEFAULT);
    vga_putchar('\n');

    if (tests_failed == 0) {
        vga_puts_color("  Todos os testes passaram!\n", THEME_SUCCESS);
    } else {
        vga_puts_color("  ATENCAO: Alguns testes falharam!\n", THEME_ERROR);
    }
    vga_puts_color("══════════════════════════════════════════════════\n\n", THEME_BORDER);
}
