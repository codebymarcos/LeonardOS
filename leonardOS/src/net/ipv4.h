// LeonardOS - IPv4 (Internet Protocol version 4)
// Envia e recebe pacotes IP, fragmentação não suportada

#ifndef __IPV4_H__
#define __IPV4_H__

#include "../common/types.h"
#include "net_config.h"
#include "ethernet.h"

// ============================================================
// Constantes IPv4
// ============================================================
#define IPV4_VERSION    4
#define IPV4_HLEN       20      // Header mínimo (sem opções)
#define IPV4_TTL        64      // Time to Live padrão

// Protocolos sobre IP
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

// ============================================================
// Header IPv4 (20 bytes mínimo)
// ============================================================
typedef struct {
    uint8_t  version_ihl;       // Version (4 bits) + IHL (4 bits)
    uint8_t  tos;               // Type of Service
    uint16_t total_length;      // Tamanho total (header + dados)
    uint16_t identification;    // ID do pacote (fragmentação)
    uint16_t flags_fragment;    // Flags (3 bits) + Fragment Offset (13 bits)
    uint8_t  ttl;               // Time to Live
    uint8_t  protocol;          // Protocolo (1=ICMP, 6=TCP, 17=UDP)
    uint16_t checksum;          // Header checksum
    uint8_t  src_ip[4];         // IP origem
    uint8_t  dst_ip[4];         // IP destino
} __attribute__((packed)) ipv4_header_t;

// ============================================================
// Callback por protocolo IP
// handler recebe payload (após header IP), tamanho, e IP origem
// ============================================================
typedef void (*ip_protocol_handler_t)(const void *payload, uint16_t len,
                                      ip_addr_t src_ip);

// ============================================================
// API pública
// ============================================================

// Inicializa a camada IPv4 (registra handler na camada Ethernet)
void ipv4_init(void);

// Envia um pacote IP
// dst_ip: endereço destino
// protocol: protocolo (IP_PROTO_ICMP, IP_PROTO_TCP, IP_PROTO_UDP)
// payload: dados a enviar
// payload_len: tamanho dos dados
// Retorna true se enviado com sucesso
bool ipv4_send(ip_addr_t dst_ip, uint8_t protocol,
               const void *payload, uint16_t payload_len);

// Registra handler para um protocolo IP específico
void ipv4_register_handler(uint8_t protocol, ip_protocol_handler_t handler);

// Calcula checksum IP (RFC 1071)
uint16_t ip_checksum(const void *data, uint16_t len);

// Estatísticas IPv4
typedef struct {
    uint32_t packets_rx;
    uint32_t packets_tx;
    uint32_t rx_bad_checksum;
    uint32_t rx_bad_version;
    uint32_t rx_not_for_us;
    uint32_t tx_no_route;
} ipv4_stats_t;

ipv4_stats_t ipv4_get_stats(void);

#endif
