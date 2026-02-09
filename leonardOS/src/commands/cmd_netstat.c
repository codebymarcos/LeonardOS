// LeonardOS - Comando: netstat
// Exibe estatisticas da interface de rede

#include "cmd_netstat.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../net/net_config.h"
#include "../drivers/net/rtl8139.h"

void cmd_netstat(const char *args) {
    (void)args;

    net_config_t *cfg = net_get_config();
    vga_putchar('\n');

    if (!cfg->nic_present) {
        vga_puts_color("  Nenhuma interface de rede ativa.\n\n", THEME_DIM);
        return;
    }

    nic_stats_t st = rtl8139_get_stats();

    vga_puts_color("  eth0 ", THEME_TITLE);
    vga_puts_color("RTL8139\n\n", THEME_DIM);

    vga_puts_color("    TX pacotes  ", THEME_LABEL);
    vga_set_color(THEME_VALUE);
    vga_putint((long)st.tx_packets);
    vga_putchar('\n');

    vga_puts_color("    TX bytes    ", THEME_LABEL);
    vga_set_color(THEME_VALUE);
    vga_putint((long)st.tx_bytes);
    vga_putchar('\n');

    vga_puts_color("    TX erros    ", THEME_LABEL);
    if (st.tx_errors > 0) {
        vga_set_color(THEME_ERROR);
    } else {
        vga_set_color(THEME_VALUE);
    }
    vga_putint((long)st.tx_errors);
    vga_putchar('\n');

    vga_putchar('\n');

    vga_puts_color("    RX pacotes  ", THEME_LABEL);
    vga_set_color(THEME_VALUE);
    vga_putint((long)st.rx_packets);
    vga_putchar('\n');

    vga_puts_color("    RX bytes    ", THEME_LABEL);
    vga_set_color(THEME_VALUE);
    vga_putint((long)st.rx_bytes);
    vga_putchar('\n');

    vga_puts_color("    RX erros    ", THEME_LABEL);
    if (st.rx_errors > 0) {
        vga_set_color(THEME_ERROR);
    } else {
        vga_set_color(THEME_VALUE);
    }
    vga_putint((long)st.rx_errors);
    vga_puts("\n\n");

    vga_set_color(THEME_DEFAULT);
}
