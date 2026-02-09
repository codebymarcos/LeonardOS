// LeonardOS - Comando: wget
// Faz HTTP GET e exibe ou salva o conteúdo
//
// Uso: wget <url>              — exibe conteúdo na tela
//      wget <url> > arquivo    — salva em arquivo (via pipe do shell)
//
// Exemplo: wget http://example.com/
//          wget http://10.0.2.2:8080/hello.txt

#include "cmd_wget.h"
#include "commands.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/types.h"
#include "../common/string.h"
#include "../net/http.h"
#include "../net/dns.h"
#include "../net/net_config.h"

void cmd_wget(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: wget <url>\n", THEME_WARNING);
        vga_puts_color("  Ex: wget http://example.com/\n", THEME_DIM);
        return;
    }

    // Verifica NIC
    net_config_t *cfg = net_get_config();
    if (!cfg->nic_present) {
        vga_puts_color("Erro: nenhuma interface de rede ativa\n", THEME_ERROR);
        return;
    }

    // Extrai URL (primeiro argumento)
    char url[256];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 255) {
        url[i] = args[i];
        i++;
    }
    url[i] = '\0';

    // Parseia URL para mostrar info
    http_url_t parsed;
    if (!http_parse_url(url, &parsed)) {
        vga_puts_color("Erro: URL invalida '", THEME_ERROR);
        vga_puts_color(url, THEME_ERROR);
        vga_puts_color("'\n", THEME_ERROR);
        vga_puts_color("  Formato: http://host[:port]/path\n", THEME_DIM);
        return;
    }

    // Mostra info
    vga_puts_color("wget ", THEME_TITLE);
    vga_puts_color(parsed.host, THEME_INFO);
    vga_puts_color(parsed.path, THEME_DIM);
    vga_putchar('\n');

    // Resolve DNS primeiro para feedback
    ip_addr_t server_ip;
    vga_puts_color("  Resolvendo ", THEME_DIM);
    vga_puts_color(parsed.host, THEME_INFO);
    vga_puts_color("... ", THEME_DIM);

    if (!dns_resolve(parsed.host, &server_ip)) {
        vga_puts_color("FALHOU\n", THEME_ERROR);
        return;
    }

    {
        char ip_str[16];
        ip_to_str(server_ip, ip_str, sizeof(ip_str));
        vga_puts_color(ip_str, THEME_VALUE);
        vga_putchar('\n');
    }

    vga_puts_color("  Conectando... ", THEME_DIM);

    // Faz request HTTP
    static http_response_t response;
    bool ok = http_get(url, &response);

    if (!ok) {
        vga_puts_color("FALHOU\n", THEME_ERROR);
        vga_puts_color("  Erro na conexao TCP ou HTTP\n", THEME_ERROR);
        return;
    }

    // Mostra resultado
    vga_puts_color("HTTP ", THEME_DEFAULT);
    vga_putint(response.status_code);

    if (response.success) {
        vga_puts_color(" OK\n", THEME_SUCCESS);
    } else {
        vga_puts_color(" ERRO\n", THEME_ERROR);
    }

    // Mostra redirecionamentos se houve
    if (response.redirect_count > 0) {
        vga_puts_color("  Redirecionamentos: ", THEME_LABEL);
        vga_putint(response.redirect_count);
        vga_putchar('\n');
        if (response.redirect_url[0]) {
            vga_puts_color("  URL final: ", THEME_LABEL);
            vga_puts_color(response.redirect_url, THEME_INFO);
            vga_putchar('\n');
        }
    }

    // Mostra Content-Length se disponível
    if (response.content_length >= 0) {
        vga_puts_color("  Tamanho: ", THEME_LABEL);
        vga_putint(response.content_length);
        vga_puts_color(" bytes", THEME_DIM);
        if (response.truncated) {
            vga_puts_color(" (truncado para ", THEME_WARNING);
            vga_putint(response.body_len);
            vga_puts_color(")", THEME_WARNING);
        }
        vga_putchar('\n');
    }

    vga_putchar('\n');

    // Exibe body como texto
    if (response.body_len > 0) {
        // Imprime body (tratando como texto, byte a byte)
        for (uint16_t j = 0; j < response.body_len; j++) {
            char c = (char)response.body[j];
            if (c == '\r') continue; // Pula CR
            if (c == '\n' || (c >= 32 && c < 127)) {
                vga_putchar(c);
            } else if (c == '\t') {
                vga_puts("    ");
            }
            // Caracteres não-printáveis são ignorados
        }

        // Garante newline no final
        if (response.body_len > 0 &&
            response.body[response.body_len - 1] != '\n') {
            vga_putchar('\n');
        }
    }
}
