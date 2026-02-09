// LeonardOS - Comando: ifconfig
// Exibe e configura interface de rede

#include "cmd_ifconfig.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../net/net_config.h"
#include "../drivers/net/rtl8139.h"

void cmd_ifconfig(const char *args) {
    net_config_t *cfg = net_get_config();

    // ifconfig ip <a.b.c.d>
    if (args && kstrncmp(args, "ip ", 3) == 0) {
        ip_addr_t new_ip;
        if (str_to_ip(args + 3, &new_ip)) {
            net_set_ip(new_ip.octets[0], new_ip.octets[1], new_ip.octets[2], new_ip.octets[3]);
            vga_puts_color("IP atualizado.\n", THEME_SUCCESS);
        } else {
            vga_puts_color("Formato invalido. Use: ifconfig ip a.b.c.d\n", THEME_ERROR);
        }
        return;
    }

    // ifconfig gw <a.b.c.d>
    if (args && kstrncmp(args, "gw ", 3) == 0) {
        ip_addr_t new_gw;
        if (str_to_ip(args + 3, &new_gw)) {
            net_set_gateway(new_gw.octets[0], new_gw.octets[1], new_gw.octets[2], new_gw.octets[3]);
            vga_puts_color("Gateway atualizado.\n", THEME_SUCCESS);
        } else {
            vga_puts_color("Formato invalido. Use: ifconfig gw a.b.c.d\n", THEME_ERROR);
        }
        return;
    }

    // ifconfig mask <a.b.c.d>
    if (args && kstrncmp(args, "mask ", 5) == 0) {
        ip_addr_t new_mask;
        if (str_to_ip(args + 5, &new_mask)) {
            net_set_netmask(new_mask.octets[0], new_mask.octets[1], new_mask.octets[2], new_mask.octets[3]);
            vga_puts_color("Netmask atualizada.\n", THEME_SUCCESS);
        } else {
            vga_puts_color("Formato invalido. Use: ifconfig mask a.b.c.d\n", THEME_ERROR);
        }
        return;
    }

    // Sem argumentos — exibe config
    vga_putchar('\n');

    if (!cfg->nic_present) {
        vga_puts_color("  Nenhuma interface de rede detectada.\n\n", THEME_DIM);
        return;
    }

    vga_puts_color("  eth0", THEME_TITLE);
    vga_puts_color("  RTL8139\n", THEME_DIM);

    char buf[18];

    vga_puts_color("    MAC       ", THEME_LABEL);
    mac_to_str(cfg->mac, buf, sizeof(buf));
    vga_puts_color(buf, THEME_VALUE);
    vga_putchar('\n');

    char ip_buf[16];

    vga_puts_color("    IP        ", THEME_LABEL);
    ip_to_str(cfg->ip, ip_buf, sizeof(ip_buf));
    vga_puts_color(ip_buf, THEME_VALUE);
    vga_putchar('\n');

    vga_puts_color("    Netmask   ", THEME_LABEL);
    ip_to_str(cfg->netmask, ip_buf, sizeof(ip_buf));
    vga_puts_color(ip_buf, THEME_VALUE);
    vga_putchar('\n');

    vga_puts_color("    Gateway   ", THEME_LABEL);
    ip_to_str(cfg->gateway, ip_buf, sizeof(ip_buf));
    vga_puts_color(ip_buf, THEME_VALUE);
    vga_putchar('\n');

    vga_puts_color("    Status    ", THEME_LABEL);
    if (cfg->configured) {
        vga_puts_color("UP", THEME_SUCCESS);
    } else {
        vga_puts_color("DOWN", THEME_ERROR);
    }
    vga_puts("\n\n");
}
