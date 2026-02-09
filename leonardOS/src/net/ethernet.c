// LeonardOS - Camada Ethernet (Layer 2)
// Monta frames, despacha pacotes recebidos por EtherType

#include "ethernet.h"
#include "../drivers/net/rtl8139.h"
#include "../net/net_config.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// ============================================================
// MAC broadcast FF:FF:FF:FF:FF:FF
// ============================================================
const uint8_t ETH_BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================
// Tabela de handlers por EtherType
// ============================================================
#define ETH_MAX_HANDLERS 8

typedef struct {
    uint16_t              ethertype;
    eth_protocol_handler_t handler;
} eth_handler_entry_t;

static eth_handler_entry_t handler_table[ETH_MAX_HANDLERS];
static int handler_count = 0;

// ============================================================
// Estatísticas
// ============================================================
static eth_stats_t stats;

eth_stats_t eth_get_stats(void) {
    return stats;
}

// ============================================================
// eth_register_handler — registra callback por EtherType
// ============================================================
void eth_register_handler(uint16_t ethertype, eth_protocol_handler_t handler) {
    if (handler_count >= ETH_MAX_HANDLERS) return;

    handler_table[handler_count].ethertype = ethertype;
    handler_table[handler_count].handler   = handler;
    handler_count++;
}

// ============================================================
// eth_rx_handler — chamado pelo RTL8139 quando um frame chega
// Faz parse do header Ethernet e despacha para o handler correto
// ============================================================
static void eth_rx_handler(const void *data, uint16_t len) {
    // Frame mínimo: header (14 bytes) + algum dado
    if (len < ETH_HLEN) {
        stats.rx_too_short++;
        return;
    }

    stats.frames_rx++;

    const eth_header_t *hdr = (const eth_header_t *)data;
    uint16_t ethertype = ntohs(hdr->ethertype);

    // Payload começa logo após o header
    const uint8_t *payload = (const uint8_t *)data + ETH_HLEN;
    uint16_t payload_len = len - ETH_HLEN;

    // Procura handler registrado para este EtherType
    for (int i = 0; i < handler_count; i++) {
        if (handler_table[i].ethertype == ethertype) {
            handler_table[i].handler(payload, payload_len, hdr->src);
            return;
        }
    }

    // Nenhum handler encontrado
    stats.rx_unknown++;
}

// ============================================================
// eth_send — monta e envia um frame Ethernet
// ============================================================
bool eth_send(const uint8_t *dst_mac, uint16_t ethertype,
              const void *payload, uint16_t payload_len) {
    if (!rtl8139_is_present()) return false;
    if (payload_len > ETH_MTU) return false;

    // Buffer estático para montar o frame
    // (não temos malloc em contexto de IRQ, e o frame é efêmero)
    static uint8_t frame_buf[ETH_FRAME_MAX];

    eth_header_t *hdr = (eth_header_t *)frame_buf;

    // MAC destino
    kmemcpy(hdr->dst, dst_mac, ETH_ALEN);

    // MAC origem (nosso)
    rtl8139_get_mac(hdr->src);

    // EtherType em big-endian
    hdr->ethertype = htons(ethertype);

    // Copia payload
    kmemcpy(frame_buf + ETH_HLEN, payload, payload_len);

    // Tamanho total do frame (mínimo 60 bytes para Ethernet)
    uint16_t frame_len = ETH_HLEN + payload_len;
    if (frame_len < ETH_FRAME_MIN) {
        // Padding com zeros
        kmemset(frame_buf + ETH_HLEN + payload_len, 0, ETH_FRAME_MIN - frame_len);
        frame_len = ETH_FRAME_MIN;
    }

    // Envia pelo RTL8139
    bool ok = rtl8139_send(frame_buf, frame_len);
    if (ok) {
        stats.frames_tx++;
    }
    return ok;
}

// ============================================================
// eth_init — inicializa a camada Ethernet
// ============================================================
void eth_init(void) {
    kmemset(&stats, 0, sizeof(stats));
    kmemset(handler_table, 0, sizeof(handler_table));
    handler_count = 0;

    // Registra nosso handler como callback de recepção no RTL8139
    if (rtl8139_is_present()) {
        rtl8139_set_rx_callback(eth_rx_handler);

        vga_puts_color("[OK] ", THEME_BOOT_OK);
        vga_puts_color("Ethernet: camada L2 ativa\n", THEME_BOOT);
    }
}
