// LeonardOS - Network Configuration
// Gerencia configuração de rede (IP, gateway, MAC) e inicialização da NIC

#include "net_config.h"
#include "../drivers/net/rtl8139.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// ============================================================
// Configuração global de rede
// ============================================================
static net_config_t config;

// ============================================================
// net_get_config — retorna ponteiro para config
// ============================================================
net_config_t *net_get_config(void) {
    return &config;
}

// ============================================================
// net_set_ip
// ============================================================
void net_set_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    config.ip.octets[0] = a;
    config.ip.octets[1] = b;
    config.ip.octets[2] = c;
    config.ip.octets[3] = d;
    config.configured = true;
}

// ============================================================
// net_set_netmask
// ============================================================
void net_set_netmask(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    config.netmask.octets[0] = a;
    config.netmask.octets[1] = b;
    config.netmask.octets[2] = c;
    config.netmask.octets[3] = d;
}

// ============================================================
// net_set_gateway
// ============================================================
void net_set_gateway(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    config.gateway.octets[0] = a;
    config.gateway.octets[1] = b;
    config.gateway.octets[2] = c;
    config.gateway.octets[3] = d;
}

// ============================================================
// net_init — detecta NIC + configura defaults
// ============================================================
void net_init(void) {
    kmemset(&config, 0, sizeof(config));

    if (rtl8139_init()) {
        config.nic_present = true;
        rtl8139_get_mac(config.mac);

        // Default: 10.0.2.15 (QEMU user-mode networking default)
        net_set_ip(10, 0, 2, 15);
        net_set_netmask(255, 255, 255, 0);
        net_set_gateway(10, 0, 2, 2);

        vga_puts_color("[OK] ", THEME_BOOT_OK);
        vga_puts_color("NIC: RTL8139 MAC=", THEME_BOOT);

        char mac_str[18];
        mac_to_str(config.mac, mac_str, sizeof(mac_str));
        vga_puts_color(mac_str, THEME_VALUE);

        vga_puts_color(" IP=", THEME_BOOT);
        char ip_str[16];
        ip_to_str(config.ip, ip_str, sizeof(ip_str));
        vga_puts_color(ip_str, THEME_VALUE);
        vga_puts("\n");
    } else {
        config.nic_present = false;
        vga_puts_color("[--] ", THEME_DIM);
        vga_puts_color("NIC: nenhuma placa de rede detectada\n", THEME_DIM);
    }
}

// ============================================================
// ip_to_str — formata IP como "a.b.c.d"
// ============================================================
void ip_to_str(ip_addr_t ip, char *buf, int max) {
    if (max < 16) return;

    int pos = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t val = ip.octets[i];

        if (val >= 100) {
            buf[pos++] = '0' + (val / 100);
            buf[pos++] = '0' + ((val / 10) % 10);
            buf[pos++] = '0' + (val % 10);
        } else if (val >= 10) {
            buf[pos++] = '0' + (val / 10);
            buf[pos++] = '0' + (val % 10);
        } else {
            buf[pos++] = '0' + val;
        }

        if (i < 3) buf[pos++] = '.';
    }
    buf[pos] = '\0';
}

// ============================================================
// str_to_ip — parse "a.b.c.d" para ip_addr_t
// ============================================================
bool str_to_ip(const char *str, ip_addr_t *out) {
    if (!str || !out) return false;

    int octet = 0;
    int val = 0;
    int digits = 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            val = val * 10 + (str[i] - '0');
            digits++;
            if (val > 255 || digits > 3) return false;
        } else if (str[i] == '.') {
            if (digits == 0 || octet >= 3) return false;
            out->octets[octet++] = (uint8_t)val;
            val = 0;
            digits = 0;
        } else {
            return false;
        }
    }

    if (digits == 0 || octet != 3) return false;
    out->octets[3] = (uint8_t)val;
    return true;
}

// ============================================================
// ip_equal — compara dois IPs
// ============================================================
bool ip_equal(ip_addr_t a, ip_addr_t b) {
    return a.octets[0] == b.octets[0] &&
           a.octets[1] == b.octets[1] &&
           a.octets[2] == b.octets[2] &&
           a.octets[3] == b.octets[3];
}

// ============================================================
// mac_to_str — formata MAC como "AA:BB:CC:DD:EE:FF"
// ============================================================
void mac_to_str(const uint8_t *mac, char *buf, int max) {
    if (max < 18) return;

    static const char hex[] = "0123456789AB";
    int pos = 0;
    for (int i = 0; i < 6; i++) {
        buf[pos++] = hex[(mac[i] >> 4) & 0x0F];
        buf[pos++] = hex[mac[i] & 0x0F];
        if (i < 5) buf[pos++] = ':';
    }
    buf[pos] = '\0';
}
