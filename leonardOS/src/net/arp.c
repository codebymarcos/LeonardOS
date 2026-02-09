// LeonardOS - ARP (Address Resolution Protocol)
// Resolve IP → MAC, mantém tabela ARP, responde a requests

#include "arp.h"
#include "ethernet.h"
#include "net_config.h"
#include "../drivers/net/rtl8139.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// ============================================================
// Tabela ARP (cache IP → MAC)
// ============================================================
static arp_entry_t arp_table[ARP_TABLE_SIZE];
static int arp_table_count = 0;

// ============================================================
// Estatísticas
// ============================================================
static arp_stats_t stats;

arp_stats_t arp_get_stats(void) {
    return stats;
}

// ============================================================
// arp_get_table — retorna tabela para exibição
// ============================================================
const arp_entry_t *arp_get_table(int *count) {
    if (count) *count = arp_table_count;
    return arp_table;
}

// ============================================================
// arp_table_lookup — busca IP na cache
// ============================================================
static arp_entry_t *arp_table_lookup(ip_addr_t ip) {
    for (int i = 0; i < arp_table_count; i++) {
        if (arp_table[i].valid && ip_equal(arp_table[i].ip, ip)) {
            return &arp_table[i];
        }
    }
    return NULL;
}

// ============================================================
// arp_table_insert — insere ou atualiza entrada
// ============================================================
static void arp_table_insert(ip_addr_t ip, const uint8_t *mac) {
    // Atualiza se já existe
    arp_entry_t *existing = arp_table_lookup(ip);
    if (existing) {
        kmemcpy(existing->mac, mac, 6);
        return;
    }

    // Insere nova entrada
    if (arp_table_count < ARP_TABLE_SIZE) {
        arp_table[arp_table_count].ip = ip;
        kmemcpy(arp_table[arp_table_count].mac, mac, 6);
        arp_table[arp_table_count].valid = true;
        arp_table_count++;
    } else {
        // Tabela cheia — sobrescreve a mais antiga (slot 0)
        arp_table[0].ip = ip;
        kmemcpy(arp_table[0].mac, mac, 6);
        arp_table[0].valid = true;
    }
}

// ============================================================
// arp_send_request — envia ARP request (broadcast)
// ============================================================
void arp_send_request(ip_addr_t target_ip) {
    net_config_t *cfg = net_get_config();
    if (!cfg->nic_present) return;

    arp_packet_t pkt;
    pkt.hw_type    = htons(ARP_HW_ETHER);
    pkt.proto_type = htons(ETHERTYPE_IPV4);
    pkt.hw_len     = 6;
    pkt.proto_len  = 4;
    pkt.opcode     = htons(ARP_OP_REQUEST);

    // Sender = nosso MAC + IP
    rtl8139_get_mac(pkt.sender_mac);
    kmemcpy(pkt.sender_ip, cfg->ip.octets, 4);

    // Target = MAC zerado (não sabemos), IP do alvo
    kmemset(pkt.target_mac, 0, 6);
    kmemcpy(pkt.target_ip, target_ip.octets, 4);

    // Envia via Ethernet broadcast
    eth_send(ETH_BROADCAST, ETHERTYPE_ARP, &pkt, sizeof(pkt));
    stats.requests_sent++;
}

// ============================================================
// arp_send_reply — responde a um ARP request
// ============================================================
static void arp_send_reply(const uint8_t *dst_mac, ip_addr_t dst_ip) {
    net_config_t *cfg = net_get_config();

    arp_packet_t pkt;
    pkt.hw_type    = htons(ARP_HW_ETHER);
    pkt.proto_type = htons(ETHERTYPE_IPV4);
    pkt.hw_len     = 6;
    pkt.proto_len  = 4;
    pkt.opcode     = htons(ARP_OP_REPLY);

    // Sender = nosso MAC + IP
    rtl8139_get_mac(pkt.sender_mac);
    kmemcpy(pkt.sender_ip, cfg->ip.octets, 4);

    // Target = quem perguntou
    kmemcpy(pkt.target_mac, dst_mac, 6);
    kmemcpy(pkt.target_ip, dst_ip.octets, 4);

    eth_send(dst_mac, ETHERTYPE_ARP, &pkt, sizeof(pkt));
    stats.replies_sent++;
}

// ============================================================
// arp_rx_handler — recebe pacotes ARP da camada Ethernet
// ============================================================
static void arp_rx_handler(const void *payload, uint16_t len,
                           const uint8_t *src_mac) {
    (void)src_mac; // Usamos o MAC do pacote ARP, não do frame

    if (len < sizeof(arp_packet_t)) return;

    const arp_packet_t *pkt = (const arp_packet_t *)payload;

    // Valida: deve ser Ethernet/IPv4
    if (ntohs(pkt->hw_type) != ARP_HW_ETHER) return;
    if (ntohs(pkt->proto_type) != ETHERTYPE_IPV4) return;
    if (pkt->hw_len != 6 || pkt->proto_len != 4) return;

    // Sempre atualiza a cache com o remetente
    ip_addr_t sender_ip;
    kmemcpy(sender_ip.octets, pkt->sender_ip, 4);
    arp_table_insert(sender_ip, pkt->sender_mac);

    uint16_t op = ntohs(pkt->opcode);

    if (op == ARP_OP_REQUEST) {
        stats.requests_received++;

        // Se o alvo somos nós, respondemos
        net_config_t *cfg = net_get_config();
        ip_addr_t target_ip;
        kmemcpy(target_ip.octets, pkt->target_ip, 4);

        if (ip_equal(target_ip, cfg->ip)) {
            arp_send_reply(pkt->sender_mac, sender_ip);
        }
    } else if (op == ARP_OP_REPLY) {
        stats.replies_received++;
        // Cache já foi atualizada acima
    }
}

// ============================================================
// arp_resolve — tenta resolver IP → MAC
// ============================================================
bool arp_resolve(ip_addr_t ip, uint8_t *mac_out) {
    // Broadcast IP → MAC broadcast
    // 255.255.255.255
    if (ip.octets[0] == 255 && ip.octets[1] == 255 &&
        ip.octets[2] == 255 && ip.octets[3] == 255) {
        kmemcpy(mac_out, ETH_BROADCAST, 6);
        return true;
    }

    // Procura na cache
    arp_entry_t *entry = arp_table_lookup(ip);
    if (entry) {
        kmemcpy(mac_out, entry->mac, 6);
        return true;
    }

    // Não encontrado — envia request
    arp_send_request(ip);
    return false;
}

// ============================================================
// arp_init — registra handler ARP na camada Ethernet
// ============================================================
void arp_init(void) {
    kmemset(&stats, 0, sizeof(stats));
    kmemset(arp_table, 0, sizeof(arp_table));
    arp_table_count = 0;

    eth_register_handler(ETHERTYPE_ARP, arp_rx_handler);

    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("ARP: protocolo registrado\n", THEME_BOOT);
}
