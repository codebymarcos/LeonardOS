// LeonardOS - Comando: nslookup
// Resolve um hostname via DNS e mostra o IP resultante
//
// Uso: nslookup <hostname>

#include "cmd_nslookup.h"
#include "commands.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/string.h"
#include "../net/dns.h"
#include "../net/net_config.h"

void cmd_nslookup(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: nslookup <hostname>\n", THEME_WARNING);
        return;
    }

    // Extrai hostname (primeiro argumento)
    char hostname[128];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 127) {
        hostname[i] = args[i];
        i++;
    }
    hostname[i] = '\0';

    vga_puts_color("Resolvendo ", THEME_DEFAULT);
    vga_puts_color(hostname, THEME_INFO);
    vga_puts_color("...\n", THEME_DEFAULT);

    ip_addr_t result;
    if (dns_resolve(hostname, &result)) {
        char ip_str[16];
        ip_to_str(result, ip_str, sizeof(ip_str));

        vga_puts_color("  Endereco: ", THEME_LABEL);
        vga_puts_color(ip_str, THEME_VALUE);
        vga_putchar('\n');
    } else {
        vga_puts_color("  Erro: nao foi possivel resolver '", THEME_ERROR);
        vga_puts_color(hostname, THEME_ERROR);
        vga_puts_color("'\n", THEME_ERROR);
    }
}
