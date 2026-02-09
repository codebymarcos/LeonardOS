// LeonardOS - ARP (Address Resolution Protocol)
// Resolve IP → MAC na rede local

#ifndef __ARP_H__
#define __ARP_H__

#include "../common/types.h"
#include "ethernet.h"
#include "net_config.h"

// ============================================================
// Constantes ARP
// ============================================================
#define ARP_HW_ETHER    1       // Hardware type: Ethernet
#define ARP_OP_REQUEST  1       // Operação: Request
#define ARP_OP_REPLY    2       // Operação: Reply

// ============================================================
// Pacote ARP (28 bytes para IPv4/Ethernet)
// ============================================================
typedef struct {
    uint16_t hw_type;           // Tipo de hardware (1 = Ethernet)
    uint16_t proto_type;        // Tipo de protocolo (0x0800 = IPv4)
    uint8_t  hw_len;            // Tamanho do endereço de hardware (6)
    uint8_t  proto_len;         // Tamanho do endereço de protocolo (4)
    uint16_t opcode;            // Operação (1=request, 2=reply)
    uint8_t  sender_mac[6];     // MAC do remetente
    uint8_t  sender_ip[4];      // IP do remetente
    uint8_t  target_mac[6];     // MAC do alvo (0 em requests)
    uint8_t  target_ip[4];      // IP do alvo
} __attribute__((packed)) arp_packet_t;

// ============================================================
// Entrada da tabela ARP
// ============================================================
#define ARP_TABLE_SIZE 16

typedef struct {
    ip_addr_t ip;
    uint8_t   mac[6];
    bool      valid;
} arp_entry_t;

// ============================================================
// API pública
// ============================================================

// Inicializa o módulo ARP (registra handler Ethernet)
void arp_init(void);

// Resolve um IP para MAC. Retorna true se encontrado na cache.
// Se não encontrado, envia ARP request e retorna false.
bool arp_resolve(ip_addr_t ip, uint8_t *mac_out);

// Envia um ARP request para um IP
void arp_send_request(ip_addr_t target_ip);

// Retorna a tabela ARP (para exibição pelo comando arp)
const arp_entry_t *arp_get_table(int *count);

// Estatísticas ARP
typedef struct {
    uint32_t requests_sent;
    uint32_t requests_received;
    uint32_t replies_sent;
    uint32_t replies_received;
} arp_stats_t;

arp_stats_t arp_get_stats(void);

#endif
