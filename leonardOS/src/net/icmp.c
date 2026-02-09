// LeonardOS - ICMP (Internet Control Message Protocol)
// Echo Request/Reply, responde a pings, suporta comando ping

#include "icmp.h"
#include "ipv4.h"
#include "ethernet.h"
#include "net_config.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// ============================================================
// Estado do ping
// ============================================================
static ping_state_t ping_state;
static icmp_stats_t stats;

ping_state_t *icmp_get_ping_state(void) {
    return &ping_state;
}

void icmp_reset_ping(void) {
    kmemset(&ping_state, 0, sizeof(ping_state));
}

icmp_stats_t icmp_get_stats(void) {
    return stats;
}

// ============================================================
// icmp_send_echo_request — envia ping
// ============================================================
bool icmp_send_echo_request(ip_addr_t dst_ip, uint16_t identifier,
                            uint16_t sequence) {
    // Monta pacote ICMP Echo Request com 32 bytes de payload
    static uint8_t icmp_buf[64];

    icmp_header_t *hdr = (icmp_header_t *)icmp_buf;
    hdr->type       = ICMP_TYPE_ECHO_REQUEST;
    hdr->code       = 0;
    hdr->checksum   = 0;
    hdr->identifier = htons(identifier);
    hdr->sequence   = htons(sequence);

    // Payload de dados (padrão: bytes incrementais, como ping real)
    uint8_t *payload = icmp_buf + sizeof(icmp_header_t);
    uint16_t payload_len = 32;
    for (uint16_t i = 0; i < payload_len; i++) {
        payload[i] = (uint8_t)(i & 0xFF);
    }

    uint16_t total_len = sizeof(icmp_header_t) + payload_len;

    // Calcula checksum do pacote ICMP inteiro
    hdr->checksum = ip_checksum(icmp_buf, total_len);

    bool ok = ipv4_send(dst_ip, IP_PROTO_ICMP, icmp_buf, total_len);
    if (ok) {
        stats.echo_requests_sent++;
    }
    return ok;
}

// ============================================================
// icmp_send_echo_reply — responde a um ping recebido
// ============================================================
static bool icmp_send_echo_reply(ip_addr_t dst_ip, uint16_t identifier,
                                 uint16_t sequence,
                                 const void *data, uint16_t data_len) {
    static uint8_t reply_buf[128];

    // Limite de segurança
    if (data_len > 100) data_len = 100;

    icmp_header_t *hdr = (icmp_header_t *)reply_buf;
    hdr->type       = ICMP_TYPE_ECHO_REPLY;
    hdr->code       = 0;
    hdr->checksum   = 0;
    hdr->identifier = identifier;  // Já em network byte order
    hdr->sequence   = sequence;    // Já em network byte order

    // Copia payload original
    kmemcpy(reply_buf + sizeof(icmp_header_t), data, data_len);

    uint16_t total_len = sizeof(icmp_header_t) + data_len;
    hdr->checksum = ip_checksum(reply_buf, total_len);

    bool ok = ipv4_send(dst_ip, IP_PROTO_ICMP, reply_buf, total_len);
    if (ok) {
        stats.echo_replies_sent++;
    }
    return ok;
}

// ============================================================
// icmp_rx_handler — recebe pacotes ICMP do IPv4
// ============================================================
static void icmp_rx_handler(const void *payload, uint16_t len,
                            ip_addr_t src_ip) {
    if (len < sizeof(icmp_header_t)) return;

    const icmp_header_t *hdr = (const icmp_header_t *)payload;

    // Verifica checksum
    uint16_t saved_cksum = hdr->checksum;
    uint8_t cksum_buf[ETH_MTU];
    if (len > ETH_MTU) return;
    kmemcpy(cksum_buf, payload, len);
    ((icmp_header_t *)cksum_buf)->checksum = 0;
    uint16_t calc_cksum = ip_checksum(cksum_buf, len);
    if (calc_cksum != saved_cksum) return;

    switch (hdr->type) {
        case ICMP_TYPE_ECHO_REQUEST:
            // Alguém nos pingou — responde
            stats.echo_requests_received++;
            {
                const uint8_t *echo_data = (const uint8_t *)payload + sizeof(icmp_header_t);
                uint16_t echo_data_len = len - sizeof(icmp_header_t);
                icmp_send_echo_reply(src_ip, hdr->identifier, hdr->sequence,
                                     echo_data, echo_data_len);
            }
            break;

        case ICMP_TYPE_ECHO_REPLY:
            // Resposta ao nosso ping
            stats.echo_replies_received++;
            {
                uint16_t reply_id = ntohs(hdr->identifier);
                uint16_t reply_seq = ntohs(hdr->sequence);

                // Verifica se é resposta ao nosso ping ativo
                if (ping_state.active && reply_id == ping_state.identifier) {
                    ping_state.reply_received = true;
                    ping_state.last_reply_seq = reply_seq;
                    ping_state.seq_received++;
                }
            }
            break;

        default:
            // Outros tipos ICMP: ignorar por enquanto
            break;
    }
}

// ============================================================
// icmp_init — registra handler ICMP no IPv4
// ============================================================
void icmp_init(void) {
    kmemset(&stats, 0, sizeof(stats));
    kmemset(&ping_state, 0, sizeof(ping_state));

    ipv4_register_handler(IP_PROTO_ICMP, icmp_rx_handler);

    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("ICMP: protocolo registrado\n", THEME_BOOT);
}
