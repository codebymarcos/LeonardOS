// LeonardOS - Network Configuration
// Configuração estática de rede (IP, gateway, netmask, MAC)

#ifndef __NET_CONFIG_H__
#define __NET_CONFIG_H__

#include "../common/types.h"

// Endereço IPv4 como 4 bytes
typedef struct {
    uint8_t octets[4];
} __attribute__((packed)) ip_addr_t;

// Configuração de rede
typedef struct {
    ip_addr_t ip;           // IP do host
    ip_addr_t netmask;      // Máscara de sub-rede
    ip_addr_t gateway;      // Gateway padrão
    uint8_t   mac[6];       // MAC address (preenchido pelo driver)
    bool      configured;   // Se a rede foi configurada
    bool      nic_present;  // Se a NIC está presente
} net_config_t;

// ============================================================
// API pública
// ============================================================

// Retorna ponteiro para a configuração de rede global
net_config_t *net_get_config(void);

// Configura IP (4 octetos separados)
void net_set_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

// Configura netmask
void net_set_netmask(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

// Configura gateway
void net_set_gateway(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

// Inicializa rede: detecta NIC, configura defaults
// Chamado pelo kernel no boot
void net_init(void);

// Formata IP como string "a.b.c.d" no buffer (mín 16 bytes)
void ip_to_str(ip_addr_t ip, char *buf, int max);

// Parse string "a.b.c.d" para ip_addr_t. Retorna true se ok.
bool str_to_ip(const char *str, ip_addr_t *out);

// Compara dois IPs
bool ip_equal(ip_addr_t a, ip_addr_t b);

// Formata MAC como string "AA:BB:CC:DD:EE:FF" (mín 18 bytes)
void mac_to_str(const uint8_t *mac, char *buf, int max);

#endif
