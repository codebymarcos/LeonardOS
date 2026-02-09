// LeonardOS - Camada Ethernet (Layer 2)
// Monta/desmonta frames Ethernet, despacha por EtherType

#ifndef __ETHERNET_H__
#define __ETHERNET_H__

#include "../common/types.h"

// ============================================================
// Constantes Ethernet
// ============================================================
#ifndef ETH_ALEN
#define ETH_ALEN        6       // Tamanho do endereço MAC (bytes)
#endif
#define ETH_HLEN        14      // Tamanho do header Ethernet (bytes)
#define ETH_MTU         1500    // Payload máximo (sem header)
#define ETH_FRAME_MIN   60      // Frame mínimo (sem CRC)
#define ETH_FRAME_MAX   1514    // Frame máximo (header + MTU, sem CRC)

// EtherTypes conhecidos
#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IPV4  0x0800

// MAC broadcast
extern const uint8_t ETH_BROADCAST[6];

// ============================================================
// Header Ethernet (14 bytes)
// ============================================================
typedef struct {
    uint8_t  dst[ETH_ALEN];    // MAC destino
    uint8_t  src[ETH_ALEN];    // MAC origem
    uint16_t ethertype;         // Tipo do protocolo (big-endian!)
} __attribute__((packed)) eth_header_t;

// ============================================================
// Callback por protocolo
// handler recebe ponteiro para payload (após header) e tamanho do payload
// ============================================================
typedef void (*eth_protocol_handler_t)(const void *payload, uint16_t len,
                                       const uint8_t *src_mac);

// ============================================================
// API pública
// ============================================================

// Inicializa a camada Ethernet (registra rx_callback no RTL8139)
void eth_init(void);

// Envia um frame Ethernet
// dst_mac: endereço destino (6 bytes)
// ethertype: protocolo (host byte order — será convertido para big-endian)
// payload: dados a enviar
// payload_len: tamanho dos dados (máx ETH_MTU)
// Retorna true se enviado com sucesso
bool eth_send(const uint8_t *dst_mac, uint16_t ethertype,
              const void *payload, uint16_t payload_len);

// Registra handler para um EtherType específico
// Quando um frame com esse ethertype chegar, o handler será chamado
void eth_register_handler(uint16_t ethertype, eth_protocol_handler_t handler);

// Estatísticas Ethernet
typedef struct {
    uint32_t frames_rx;       // Frames recebidos
    uint32_t frames_tx;       // Frames enviados
    uint32_t rx_too_short;    // Frames recebidos muito curtos
    uint32_t rx_unknown;      // EtherType desconhecido
} eth_stats_t;

eth_stats_t eth_get_stats(void);

// ============================================================
// Utilidades de byte order (network = big-endian, host = little-endian)
// ============================================================
static inline uint16_t htons(uint16_t h) {
    return (h >> 8) | (h << 8);
}

static inline uint16_t ntohs(uint16_t n) {
    return (n >> 8) | (n << 8);
}

static inline uint32_t htonl(uint32_t h) {
    return ((h & 0xFF000000) >> 24) |
           ((h & 0x00FF0000) >> 8)  |
           ((h & 0x0000FF00) << 8)  |
           ((h & 0x000000FF) << 24);
}

static inline uint32_t ntohl(uint32_t n) {
    return htonl(n);
}

#endif
