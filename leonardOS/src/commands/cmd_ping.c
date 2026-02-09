// LeonardOS - Comando: ping
// Envia ICMP Echo Requests e exibe respostas
//
// Uso: ping <ip>          — envia 4 pings
//      ping <ip> <count>  — envia N pings
//
// Como não temos timer ISR (PIT), usamos busy-wait com PIT counter
// para medir tempo e timeout.

#include "cmd_ping.h"
#include "commands.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../common/types.h"
#include "../common/string.h"
#include "../common/io.h"
#include "../net/net_config.h"
#include "../net/icmp.h"
#include "../net/arp.h"
#include "../drivers/timer/pit.h"

// ============================================================
// Delay preciso via PIT timer
// ============================================================
static void ping_delay_ms(uint32_t ms) {
    pit_sleep_ms(ms);
}

// Espera um reply ICMP com timeout (~2 segundos)
// Retorna true se reply recebido
static bool ping_wait_reply(uint32_t timeout_ms) {
    ping_state_t *state = icmp_get_ping_state();

    // Polling loop com delay granular
    for (uint32_t elapsed = 0; elapsed < timeout_ms; elapsed += 10) {
        if (state->reply_received) {
            return true;
        }
        ping_delay_ms(10);
    }
    return state->reply_received;
}

// Parse simples de número
static int parse_int(const char *s) {
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

// ============================================================
// cmd_ping — implementação do comando
// ============================================================
void cmd_ping(const char *args) {
    if (!args || args[0] == '\0') {
        vga_puts_color("Uso: ping <ip> [count]\n", THEME_WARNING);
        return;
    }

    // Verifica se NIC está presente
    net_config_t *cfg = net_get_config();
    if (!cfg->nic_present) {
        vga_puts_color("Erro: nenhuma interface de rede ativa\n", THEME_ERROR);
        return;
    }

    // Parse IP
    char ip_str[64];
    int i = 0;
    while (args[i] && args[i] != ' ' && i < 63) {
        ip_str[i] = args[i];
        i++;
    }
    ip_str[i] = '\0';

    ip_addr_t target;
    if (!str_to_ip(ip_str, &target)) {
        vga_puts_color("Erro: IP invalido '", THEME_ERROR);
        vga_puts_color(ip_str, THEME_ERROR);
        vga_puts_color("'\n", THEME_ERROR);
        return;
    }

    // Parse count (opcional, default 4)
    int count = 4;
    while (args[i] == ' ') i++;
    if (args[i] >= '0' && args[i] <= '9') {
        count = parse_int(&args[i]);
        if (count < 1) count = 1;
        if (count > 100) count = 100;
    }

    // Header
    char target_str[16];
    ip_to_str(target, target_str, sizeof(target_str));

    vga_puts_color("PING ", THEME_TITLE);
    vga_puts_color(target_str, THEME_INFO);
    vga_puts_color(": ", THEME_DEFAULT);
    vga_putint(count);
    vga_puts_color(" pacotes\n", THEME_DEFAULT);

    // Prepara estado do ping
    icmp_reset_ping();
    ping_state_t *state = icmp_get_ping_state();
    state->active = true;
    state->target = target;
    state->identifier = 0x4C4F; // "LO" — LeonardOS
    state->seq_sent = 0;
    state->seq_received = 0;

    // Primeiro, garante que temos o MAC do gateway/target via ARP
    // Envia ARP request e espera um pouco
    ip_addr_t next_hop = target;

    // Verifica se destino está na mesma sub-rede
    bool same_net = true;
    for (int j = 0; j < 4; j++) {
        if ((target.octets[j] & cfg->netmask.octets[j]) !=
            (cfg->ip.octets[j]  & cfg->netmask.octets[j])) {
            same_net = false;
            break;
        }
    }
    if (!same_net) {
        next_hop = cfg->gateway;
    }

    // Tenta resolver ARP antes (envia 2 requests com delay)
    uint8_t dummy_mac[6];
    if (!arp_resolve(next_hop, dummy_mac)) {
        ping_delay_ms(200);
        // Tenta de novo
        if (!arp_resolve(next_hop, dummy_mac)) {
            ping_delay_ms(500);
        }
    }

    // Envia pings
    int sent = 0;
    int received = 0;

    for (int seq = 1; seq <= count; seq++) {
        state->reply_received = false;
        state->seq_sent = seq;

        bool ok = icmp_send_echo_request(target, state->identifier, (uint16_t)seq);
        if (!ok) {
            vga_puts_color("  Erro ao enviar pacote ", THEME_ERROR);
            vga_putint(seq);
            vga_puts_color(" (sem rota ARP)\n", THEME_ERROR);

            // Pode ser que o ARP ainda não resolveu, tenta de novo
            if (seq == 1) {
                ping_delay_ms(500);
                ok = icmp_send_echo_request(target, state->identifier, (uint16_t)seq);
            }

            if (!ok) {
                sent++;
                // Delay entre pings
                if (seq < count) ping_delay_ms(1000);
                continue;
            }
        }

        sent++;

        // Espera reply com timeout de 2 segundos
        bool got_reply = ping_wait_reply(2000);

        if (got_reply) {
            received++;
            vga_puts_color("  Resposta de ", THEME_DEFAULT);
            vga_puts_color(target_str, THEME_INFO);
            vga_puts_color(": seq=", THEME_DEFAULT);
            vga_putint(seq);
            vga_puts_color(" ttl=64", THEME_DEFAULT);
            vga_putchar('\n');
        } else {
            vga_puts_color("  Timeout: seq=", THEME_DIM);
            vga_putint(seq);
            vga_putchar('\n');
        }

        // Delay entre pings (~1s)
        if (seq < count) {
            ping_delay_ms(1000);
        }
    }

    // Resumo
    vga_puts_color("\n--- ", THEME_DIM);
    vga_puts_color(target_str, THEME_INFO);
    vga_puts_color(" ---\n", THEME_DIM);

    vga_putint(sent);
    vga_puts_color(" enviados, ", THEME_DEFAULT);

    if (received > 0) {
        vga_set_color(THEME_SUCCESS);
    } else {
        vga_set_color(THEME_ERROR);
    }
    vga_putint(received);
    vga_puts(" recebidos");
    vga_set_color(THEME_DEFAULT);

    if (sent > 0) {
        int loss = ((sent - received) * 100) / sent;
        vga_puts(", ");
        vga_putint(loss);
        vga_puts("% perda");
    }
    vga_putchar('\n');

    // Limpa estado
    state->active = false;
}
