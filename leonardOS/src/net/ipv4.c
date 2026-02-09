// LeonardOS - IPv4 (Internet Protocol version 4)
// Envia/recebe pacotes IP, checksum, dispatch por protocolo

#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "net_config.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// ============================================================
// Tabela de handlers por protocolo IP
// ============================================================
#define IP_MAX_HANDLERS 8

typedef struct {
    uint8_t               protocol;
    ip_protocol_handler_t handler;
} ip_handler_entry_t;

static ip_handler_entry_t ip_handlers[IP_MAX_HANDLERS];
static int ip_handler_count = 0;

// ============================================================
// Estatísticas + ID counter
// ============================================================
static ipv4_stats_t stats;
static uint16_t ip_id_counter = 1;

ipv4_stats_t ipv4_get_stats(void) {
    return stats;
}

// ============================================================
// ip_checksum — RFC 1071, one's complement sum
// ============================================================
uint16_t ip_checksum(const void *data, uint16_t len) {
    const uint16_t *words = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *words++;
        len -= 2;
    }

    // Byte ímpar restante
    if (len == 1) {
        uint16_t last = 0;
        *((uint8_t *)&last) = *((const uint8_t *)words);
        sum += last;
    }

    // Fold carries
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

// ============================================================
// ipv4_register_handler — registra callback por protocolo
// ============================================================
void ipv4_register_handler(uint8_t protocol, ip_protocol_handler_t handler) {
    if (ip_handler_count >= IP_MAX_HANDLERS) return;

    ip_handlers[ip_handler_count].protocol = protocol;
    ip_handlers[ip_handler_count].handler  = handler;
    ip_handler_count++;
}

// ============================================================
// ipv4_rx_handler — chamado pela camada Ethernet (EtherType 0x0800)
// ============================================================
static void ipv4_rx_handler(const void *payload, uint16_t len,
                            const uint8_t *src_mac) {
    (void)src_mac;

    if (len < IPV4_HLEN) return;

    const ipv4_header_t *hdr = (const ipv4_header_t *)payload;

    // Verifica versão
    uint8_t version = (hdr->version_ihl >> 4) & 0x0F;
    if (version != 4) {
        stats.rx_bad_version++;
        return;
    }

    // Calcula tamanho do header (IHL * 4 bytes)
    uint8_t ihl = (hdr->version_ihl & 0x0F) * 4;
    if (ihl < IPV4_HLEN || ihl > len) return;

    // Verifica checksum
    uint16_t saved_cksum = hdr->checksum;
    // Copia header para verificar (precisa zerar checksum)
    uint8_t hdr_copy[60]; // Max IHL = 15*4 = 60
    kmemcpy(hdr_copy, hdr, ihl);
    ((ipv4_header_t *)hdr_copy)->checksum = 0;
    uint16_t calc_cksum = ip_checksum(hdr_copy, ihl);
    if (calc_cksum != saved_cksum) {
        stats.rx_bad_checksum++;
        return;
    }

    // Verifica se o pacote é para nós
    net_config_t *cfg = net_get_config();
    ip_addr_t dst_ip;
    kmemcpy(dst_ip.octets, hdr->dst_ip, 4);

    bool for_us = ip_equal(dst_ip, cfg->ip);
    // Aceita broadcast também (255.255.255.255)
    if (!for_us) {
        if (dst_ip.octets[0] == 255 && dst_ip.octets[1] == 255 &&
            dst_ip.octets[2] == 255 && dst_ip.octets[3] == 255) {
            for_us = true;
        }
    }

    if (!for_us) {
        stats.rx_not_for_us++;
        return;
    }

    stats.packets_rx++;

    // Extrai IP origem
    ip_addr_t src_ip;
    kmemcpy(src_ip.octets, hdr->src_ip, 4);

    // Payload IP começa após o header
    const uint8_t *ip_payload = (const uint8_t *)payload + ihl;
    uint16_t ip_payload_len = ntohs(hdr->total_length) - ihl;
    if (ip_payload_len > len - ihl) {
        ip_payload_len = len - ihl; // Segurança
    }

    // Despacha para handler do protocolo
    for (int i = 0; i < ip_handler_count; i++) {
        if (ip_handlers[i].protocol == hdr->protocol) {
            ip_handlers[i].handler(ip_payload, ip_payload_len, src_ip);
            return;
        }
    }
}

// ============================================================
// ip_determine_dst_mac — resolve MAC para enviar pacote
// Se o destino está na mesma rede, resolve diretamente.
// Caso contrário, usa o gateway.
// ============================================================
static bool ip_determine_next_hop(ip_addr_t dst_ip, ip_addr_t *next_hop) {
    net_config_t *cfg = net_get_config();

    // Verifica se destino está na mesma sub-rede
    bool same_net = true;
    for (int i = 0; i < 4; i++) {
        if ((dst_ip.octets[i] & cfg->netmask.octets[i]) !=
            (cfg->ip.octets[i]  & cfg->netmask.octets[i])) {
            same_net = false;
            break;
        }
    }

    if (same_net) {
        *next_hop = dst_ip;
    } else {
        *next_hop = cfg->gateway;
    }
    return true;
}

// ============================================================
// ipv4_send — monta e envia um pacote IP
// ============================================================
bool ipv4_send(ip_addr_t dst_ip, uint8_t protocol,
               const void *payload, uint16_t payload_len) {
    net_config_t *cfg = net_get_config();
    if (!cfg->nic_present || !cfg->configured) return false;

    // Limite: payload + header deve caber no MTU Ethernet
    if (payload_len > ETH_MTU - IPV4_HLEN) return false;

    // Monta header IP
    static uint8_t pkt_buf[ETH_MTU];
    ipv4_header_t *hdr = (ipv4_header_t *)pkt_buf;

    hdr->version_ihl   = (IPV4_VERSION << 4) | (IPV4_HLEN / 4);
    hdr->tos            = 0;
    hdr->total_length   = htons(IPV4_HLEN + payload_len);
    hdr->identification = htons(ip_id_counter++);
    hdr->flags_fragment = htons(0x4000); // Don't Fragment
    hdr->ttl            = IPV4_TTL;
    hdr->protocol       = protocol;
    hdr->checksum       = 0;
    kmemcpy(hdr->src_ip, cfg->ip.octets, 4);
    kmemcpy(hdr->dst_ip, dst_ip.octets, 4);

    // Calcula checksum do header
    hdr->checksum = ip_checksum(hdr, IPV4_HLEN);

    // Copia payload
    kmemcpy(pkt_buf + IPV4_HLEN, payload, payload_len);

    // Determina next-hop e resolve MAC
    ip_addr_t next_hop;
    ip_determine_next_hop(dst_ip, &next_hop);

    uint8_t dst_mac[6];
    if (!arp_resolve(next_hop, dst_mac)) {
        // ARP request enviado, pacote perdido (simplificação: sem fila)
        stats.tx_no_route++;
        return false;
    }

    // Envia via Ethernet
    bool ok = eth_send(dst_mac, ETHERTYPE_IPV4, pkt_buf, IPV4_HLEN + payload_len);
    if (ok) {
        stats.packets_tx++;
    }
    return ok;
}

// ============================================================
// ipv4_init — registra handler IPv4 na camada Ethernet
// ============================================================
void ipv4_init(void) {
    kmemset(&stats, 0, sizeof(stats));
    kmemset(ip_handlers, 0, sizeof(ip_handlers));
    ip_handler_count = 0;
    ip_id_counter = 1;

    eth_register_handler(ETHERTYPE_IPV4, ipv4_rx_handler);

    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("IPv4: protocolo registrado\n", THEME_BOOT);
}
