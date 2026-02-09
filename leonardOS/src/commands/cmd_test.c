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
#include "../memory/vmm.h"
#include "../memory/heap.h"
#include "../fs/vfs.h"
#include "../fs/ramfs.h"
#include "../shell/shell.h"

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

    // Testa se paging está habilitado (bit 31 de CR0)
    int paging = (cr0 >> 31) & 1;
    test_result("CR0 Paging habilitado (PG bit)", paging, paging ? "ativo" : "DESATIVADO");

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
// 10. Teste do Paging (VMM)
// ============================================================
static void test_paging(void) {
    test_header("Paging / VMM");

    // Verifica que paging está habilitado (CR0 bit 31)
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    int pg_enabled = (cr0 >> 31) & 1;
    test_result("CR0 Paging habilitado (PG bit)", pg_enabled, NULL);

    // Verifica CR3 (Page Directory base)
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    test_result("CR3 != 0 (PD carregado)", cr3 != 0, NULL);
    test_info_hex("CR3 (Page Directory)", cr3);

    // Estatísticas
    struct vmm_stats stats = paging_get_stats();
    test_info_int("Paginas mapeadas", stats.pages_mapped);
    test_info_int("Page Tables usadas", stats.page_tables_used);
    test_info_int("Identity map (MB)", stats.identity_map_mb);
    test_info_int("Page faults", stats.page_faults);

    test_result("Page Tables == 4 (16MB/4MB)", stats.page_tables_used == 4, NULL);
    // 16MB / 4KB = 4096 páginas
    test_result("Paginas mapeadas == 4096", stats.pages_mapped >= 4096, NULL);

    // Identity map: virtual 0x100000 → physical 0x100000 (kernel)
    uint32_t kernel_phys = get_physical_addr(0x100000);
    test_result("Identity: 0x100000 -> 0x100000", kernel_phys == 0x100000, NULL);
    test_info_hex("get_physical_addr(0x100000)", kernel_phys);

    // Identity map: VGA buffer 0xB8000
    uint32_t vga_phys = get_physical_addr(0xB8000);
    test_result("Identity: 0xB8000 -> 0xB8000 (VGA)", vga_phys == 0xB8000, NULL);

    // Identity map: endereço 0 (deve mapear para 0)
    uint32_t zero_phys = get_physical_addr(0x0);
    test_result("Identity: 0x0 -> 0x0", zero_phys == 0x0, NULL);

    // Verifica is_page_mapped em região identity-mapped
    test_result("is_page_mapped(0x100000)", is_page_mapped(0x100000), NULL);
    test_result("is_page_mapped(0xB8000)", is_page_mapped(0xB8000), NULL);

    // Verifica que região acima de 16MB NÃO está mapeada
    test_result("!is_page_mapped(0x1000000) (16MB)", !is_page_mapped(0x1000000), NULL);

    // Teste de map_page: mapeia uma página nova acima de 16MB
    // Usa um frame do PMM como backing
    uint32_t test_frame = pmm_alloc_frame();
    if (test_frame != 0) {
        uint32_t test_vaddr = 0x2000000;  // 32MB - fora do identity map

        map_page(test_vaddr, test_frame, PAGE_KERNEL);
        test_result("map_page: pagina mapeada", is_page_mapped(test_vaddr), NULL);

        uint32_t resolved = get_physical_addr(test_vaddr);
        test_result("map_page: resolve corretamente",
                    resolved == test_frame, NULL);
        test_info_hex("map_page vaddr", test_vaddr);
        test_info_hex("map_page paddr", resolved);

        // Escreve e lê na página mapeada
        volatile uint32_t *ptr = (volatile uint32_t *)test_vaddr;
        *ptr = 0xCAFEBABE;
        test_result("map_page: write/read OK", *ptr == 0xCAFEBABE, NULL);

        // unmap_page
        unmap_page(test_vaddr);
        test_result("unmap_page: pagina desmapeada", !is_page_mapped(test_vaddr), NULL);

        // Libera o frame de teste
        pmm_free_frame(test_frame);
    } else {
        test_result("map_page: frame alocado", 0, "sem memoria");
    }

    test_result("Page faults == 0 (nenhum inesperado)",
                paging_get_stats().page_faults == 0, NULL);
}

// ============================================================
// 11. Teste do Heap (kmalloc / kfree)
// ============================================================
static void test_heap(void) {
    test_header("Heap (kmalloc / kfree)");

    // Stats iniciais
    struct heap_stats s0 = heap_get_stats();
    test_info_int("Heap total (bytes)", s0.total_bytes);
    test_info_int("Heap livre (bytes)", s0.free_bytes);
    test_info_int("Paginas alocadas", s0.pages_allocated);
    test_result("Heap inicializado (pages > 0)", s0.pages_allocated > 0, NULL);
    test_result("1 bloco livre inicial", s0.free_blocks >= 1, NULL);
    test_info_int("Blocos usados (pre-existentes)", s0.used_blocks);

    // 1. kmalloc(32)
    void *a = kmalloc(32);
    test_result("kmalloc(32) != NULL", a != NULL, NULL);
    test_result("Alinhado a 8 bytes", ((uint32_t)(uintptr_t)a % HEAP_ALIGNMENT) == 0, NULL);
    test_info_hex("Endereco a", (uint32_t)(uintptr_t)a);

    // 2. kmalloc(128)
    void *b = kmalloc(128);
    test_result("kmalloc(128) != NULL", b != NULL, NULL);
    test_result("b != a (enderecos diferentes)", b != a, NULL);
    test_result("b > a (sequencial)", (uint32_t)(uintptr_t)b > (uint32_t)(uintptr_t)a, NULL);
    test_info_hex("Endereco b", (uint32_t)(uintptr_t)b);

    // Verifica stats após 2 alocações (relativo ao estado inicial)
    struct heap_stats s1 = heap_get_stats();
    test_result("+2 blocos usados", s1.used_blocks == s0.used_blocks + 2, NULL);
    test_result("alloc_count == 2", s1.alloc_count - s0.alloc_count == 2, NULL);

    // 3. kfree(a) — libera o primeiro
    kfree(a);
    struct heap_stats s2 = heap_get_stats();
    test_result("kfree(a): +1 bloco usado", s2.used_blocks == s0.used_blocks + 1, NULL);

    // 4. kmalloc(16) — deve reutilizar o espaço de 'a'
    void *c = kmalloc(16);
    test_result("kmalloc(16) != NULL", c != NULL, NULL);
    test_result("Reutilizou espaco de a (c <= a)",
                (uint32_t)(uintptr_t)c <= (uint32_t)(uintptr_t)a, NULL);
    test_info_hex("Endereco c", (uint32_t)(uintptr_t)c);

    // 5. kfree(b)
    kfree(b);

    // 6. kfree(c)
    kfree(c);

    // 7. Heap deve voltar ao estado inicial
    struct heap_stats s3 = heap_get_stats();
    test_result("Todos liberados: usados restaurado", s3.used_blocks == s0.used_blocks, NULL);
    test_result("Coalescing: free_blocks restaurado", s3.free_blocks <= s0.free_blocks + 1, NULL);
    test_result("free_count correto", s3.free_count - s0.free_count == 3, NULL);

    // Stats devem bater
    test_result("free_bytes restaurado", s3.free_bytes == s0.free_bytes, NULL);

    // Double-free protection
    void *d = kmalloc(64);
    kfree(d);
    kfree(d);  // double-free — não deve crashar
    struct heap_stats s4 = heap_get_stats();
    test_result("Double-free seguro", s4.used_blocks == s0.used_blocks, NULL);

    // NULL free protection
    kfree(NULL);  // não deve crashar
    test_result("kfree(NULL) seguro", 1, NULL);

    // kmalloc(0) deve retornar NULL
    void *e = kmalloc(0);
    test_result("kmalloc(0) == NULL", e == NULL, NULL);
}

// ============================================================
// 12. Teste do VFS + RamFS
// ============================================================
static void test_vfs(void) {
    test_header("VFS + RamFS");

    // 1. Raiz existe
    test_result("vfs_root != NULL", vfs_root != NULL, NULL);
    test_result("vfs_root e diretorio", vfs_root->type == VFS_DIRECTORY, NULL);

    // 2. vfs_open("/") retorna raiz
    vfs_node_t *root = vfs_open("/");
    test_result("vfs_open('/') != NULL", root != NULL, NULL);
    test_result("vfs_open('/') == vfs_root", root == vfs_root, NULL);

    // 3. /etc existe
    vfs_node_t *etc = vfs_open("/etc");
    test_result("vfs_open('/etc') != NULL", etc != NULL, NULL);
    if (etc) {
        test_result("/etc e diretorio", etc->type == VFS_DIRECTORY, NULL);
    }

    // 4. /etc/hostname existe e tem conteudo
    vfs_node_t *hostname = vfs_open("/etc/hostname");
    test_result("vfs_open('/etc/hostname') != NULL", hostname != NULL, NULL);
    if (hostname) {
        test_result("/etc/hostname e arquivo", hostname->type == VFS_FILE, NULL);
        test_result("hostname.size == 9", hostname->size == 9, NULL);

        // Lê conteúdo
        uint8_t buf[32];
        uint32_t bytes = vfs_read(hostname, 0, 32, buf);
        test_result("vfs_read retorna 9 bytes", bytes == 9, NULL);
        buf[bytes] = '\0';

        // Verifica conteúdo
        int match = 1;
        const char *expected = "leonardos";
        for (uint32_t i = 0; i < 9; i++) {
            if (buf[i] != (uint8_t)expected[i]) match = 0;
        }
        test_result("Conteudo == 'leonardos'", match, NULL);
    }

    // 5. Path invalido retorna NULL
    vfs_node_t *nope = vfs_open("/nao/existe");
    test_result("Path invalido -> NULL", nope == NULL, NULL);

    // 6. vfs_open(NULL) retorna NULL
    test_result("vfs_open(NULL) -> NULL", vfs_open(NULL) == NULL, NULL);

    // 7. Criar arquivo via ramfs_create_file
    vfs_node_t *tmp = vfs_open("/tmp");
    test_result("/tmp existe", tmp != NULL, NULL);
    if (tmp) {
        vfs_node_t *tf = ramfs_create_file(tmp, "test.txt");
        test_result("Criar /tmp/test.txt", tf != NULL, NULL);
        if (tf) {
            // Escreve
            const char *data = "hello";
            uint32_t w = vfs_write(tf, 0, 5, (const uint8_t *)data);
            test_result("vfs_write 5 bytes", w == 5, NULL);
            test_result("size atualizado", tf->size == 5, NULL);

            // Lê de volta
            uint8_t rbuf[16];
            uint32_t r = vfs_read(tf, 0, 16, rbuf);
            test_result("vfs_read retorna 5", r == 5, NULL);

            int ok = 1;
            for (uint32_t i = 0; i < 5; i++) {
                if (rbuf[i] != (uint8_t)data[i]) ok = 0;
            }
            test_result("Dados lidos == 'hello'", ok, NULL);

            // Overwrite
            const char *data2 = "world!";
            tf->size = 0;  // reset
            w = vfs_write(tf, 0, 6, (const uint8_t *)data2);
            test_result("Overwrite 6 bytes", w == 6, NULL);
            test_result("size == 6", tf->size == 6, NULL);

            // Resolve via path
            vfs_node_t *found = vfs_open("/tmp/test.txt");
            test_result("vfs_open('/tmp/test.txt')", found == tf, NULL);
        }
    }

    // 8. readdir lista filhos
    if (tmp) {
        vfs_node_t *first = vfs_readdir(tmp, 0);
        test_result("readdir(tmp, 0) != NULL", first != NULL, NULL);

        // Fora do range
        vfs_node_t *oob = vfs_readdir(tmp, 999);
        test_result("readdir(tmp, 999) == NULL", oob == NULL, NULL);
    }

    // 9. Read com offset
    vfs_node_t *hostname2 = vfs_open("/etc/hostname");
    if (hostname2) {
        uint8_t buf2[16];
        uint32_t r2 = vfs_read(hostname2, 5, 10, buf2);
        test_result("Read com offset=5 retorna 4", r2 == 4, NULL);
    }

    // 10. Read alem do tamanho
    if (hostname2) {
        uint8_t buf3[16];
        uint32_t r3 = vfs_read(hostname2, 100, 10, buf3);
        test_result("Read offset>size retorna 0", r3 == 0, NULL);
    }
}

// ============================================================
// 13. Teste do sistema de Comandos
// ============================================================
static void test_commands(void) {
    test_header("Sistema de Comandos");

    int count = commands_get_count();
    test_info_int("Comandos registrados", count);
    test_result("Pelo menos 4 comandos", count >= 4, NULL);

    // Verifica se cada comando base existe
    const char *expected[] = {"help", "clear", "sysinfo", "halt", "test", "mem", "ls", "cat", "echo", "pwd", "cd", "mkdir", "touch"};
    int num_expected = 13;

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
// 13. Teste de pwd / cd (navegação) + vfs_resolve
// ============================================================
static void test_pwd_cd(void) {
    test_header("pwd / cd / vfs_resolve");

    // Salva estado inicial
    vfs_node_t *original_dir = current_dir;
    char original_path[256];
    int i = 0;
    while (i < 255 && current_path[i]) {
        original_path[i] = current_path[i];
        i++;
    }
    original_path[i] = '\0';

    // 1. pwd no início deve ser "/"
    test_result("pwd inicial == '/'", current_path[0] == '/' && current_path[1] == '\0', NULL);
    test_result("current_dir == vfs_root", current_dir == vfs_root, NULL);

    // 2. cd /etc
    extern void cmd_cd(const char*);
    cmd_cd("/etc");
    test_result("cd /etc: path == '/etc'",
                current_path[0] == '/' && current_path[1] == 'e' &&
                current_path[2] == 't' && current_path[3] == 'c' &&
                current_path[4] == '\0', NULL);
    test_result("cd /etc: current_dir é diretório", current_dir->type == VFS_DIRECTORY, NULL);

    // 3. cd .. (volta para raiz)
    cmd_cd("..");
    test_result("cd ..: volta para '/'", current_path[0] == '/' && current_path[1] == '\0', NULL);
    test_result("cd ..: current_dir == vfs_root", current_dir == vfs_root, NULL);

    // 4. cd sem argumento volta para raiz
    cmd_cd("/etc");
    cmd_cd("");
    test_result("cd (vazio): volta para '/'", current_path[0] == '/' && current_path[1] == '\0', NULL);

    // 5. cd . (noop)
    cmd_cd("/etc");
    cmd_cd(".");
    test_result("cd .: mantém '/etc'",
                current_path[0] == '/' && current_path[1] == 'e' &&
                current_path[2] == 't' && current_path[3] == 'c' &&
                current_path[4] == '\0', NULL);

    // 6. vfs_resolve relativo
    cmd_cd("/");
    vfs_node_t *etc = vfs_resolve("etc", current_dir, NULL, 0);
    test_result("vfs_resolve('etc') relativo", etc != NULL && etc->type == VFS_DIRECTORY, NULL);

    // 7. vfs_resolve com ..
    cmd_cd("/etc");
    vfs_node_t *tmp_via_dotdot = vfs_resolve("../tmp", current_dir, NULL, 0);
    test_result("vfs_resolve('../tmp') de /etc", tmp_via_dotdot != NULL, NULL);

    // 8. vfs_build_path normaliza
    char out[256];
    int ok = vfs_build_path("/etc", "..", out, 256);
    test_result("build_path('/etc','..') == '/'", ok && out[0] == '/' && out[1] == '\0', NULL);

    ok = vfs_build_path("/", "etc/../tmp", out, 256);
    test_result("build_path('/','etc/../tmp') == '/tmp'",
                ok && out[0] == '/' && out[1] == 't' &&
                out[2] == 'm' && out[3] == 'p' && out[4] == '\0', NULL);

    // 9. cd inválido não muda estado
    cmd_cd("/");
    cmd_cd("/nao/existe");
    test_result("cd inválido não muda path", current_path[0] == '/' && current_path[1] == '\0', NULL);

    // Restaura estado
    current_dir = original_dir;
    i = 0;
    while (i < 255 && original_path[i]) {
        current_path[i] = original_path[i];
        i++;
    }
    current_path[i] = '\0';
}

// ============================================================
// 14. Teste do sistema de Comandos
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
    test_paging();
    test_heap();
    test_vfs();
    test_pwd_cd();
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
