// LeonardOS - Comando: keytest
// Mostra o scancode bruto de cada tecla pressionada
// Útil para mapear teclados ABNT2 e outros layouts
//
// Uso: keytest     → pressione teclas, ESC para sair

#include "cmd_keytest.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../common/colors.h"

void cmd_keytest(const char *args) {
    (void)args;

    vga_puts_color("═══ Diagnostico de Teclado ═══\n", THEME_TITLE);
    vga_puts_color("Pressione teclas para ver scancodes.\n", THEME_DIM);
    vga_puts_color("ESC para sair.\n\n", THEME_DIM);
    vga_puts_color("Scancode  Hex    Tipo\n", THEME_LABEL);
    vga_puts_color("────────────────────────────────\n", THEME_BORDER);

    kbd_set_raw_mode(1);

    while (1) {
        // Espera uma tecla (bloqueia)
        kbd_getchar();  // consome o dummy

        unsigned char sc = kbd_get_raw_scancode();
        if (sc == 0) continue;

        // ESC press = scancode 0x01
        if (sc == 0x01) break;

        // Mostra scancode
        vga_puts_color("  ", THEME_DEFAULT);

        // Decimal
        int d = (int)sc;
        if (sc & 0x80) d = (int)(sc & 0x7F);  // mostra base para releases
        char num[4];
        num[0] = '0' + (d / 100) % 10;
        num[1] = '0' + (d / 10) % 10;
        num[2] = '0' + d % 10;
        num[3] = '\0';
        vga_puts_color(num, THEME_VALUE);
        vga_puts("     ");

        // Hexadecimal
        vga_puts_color("0x", THEME_DIM);
        char hex[3];
        int hi = (sc >> 4) & 0x0F;
        int lo = sc & 0x0F;
        hex[0] = (hi < 10) ? ('0' + hi) : ('A' + hi - 10);
        hex[1] = (lo < 10) ? ('0' + lo) : ('A' + lo - 10);
        hex[2] = '\0';
        vga_puts_color(hex, THEME_VALUE);
        vga_puts("   ");

        // Press ou Release
        if (sc & 0x80) {
            vga_puts_color("release", THEME_DIM);
        } else {
            vga_puts_color("PRESS", THEME_SUCCESS);

            // Mostra o que o mapa atual produz
            vga_puts("  -> mapa: ");
            // Não temos acesso direto aos mapas (são static)
            // Mas mostramos o index
            vga_puts_color("[", THEME_DIM);
            vga_putint(sc);
            vga_puts_color("]", THEME_DIM);
        }

        vga_putchar('\n');
    }

    kbd_set_raw_mode(0);

    vga_puts_color("\nDiagnostico encerrado.\n", THEME_INFO);
}
