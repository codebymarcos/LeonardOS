// LeonardOS - DNS Resolver
// Resolve hostnames via UDP port 53 (QEMU DNS: 10.0.2.3)

#ifndef __DNS_H__
#define __DNS_H__

#include "../common/types.h"
#include "net_config.h"

// ============================================================
// Constantes DNS
// ============================================================
#define DNS_PORT            53
#define DNS_MAX_NAME        128     // Tamanho máximo do hostname
#define DNS_CACHE_SIZE      16      // Entradas no cache
#define DNS_TIMEOUT_MS      3000    // Timeout de query (3s)

// Tipos de registro
#define DNS_TYPE_A          1       // Registro A (IPv4)
#define DNS_CLASS_IN        1       // Classe Internet

// Flags de resposta
#define DNS_FLAG_QR         0x8000  // Query/Response
#define DNS_FLAG_RD         0x0100  // Recursion Desired
#define DNS_FLAG_RA         0x0080  // Recursion Available
#define DNS_FLAG_RCODE      0x000F  // Response code mask

// ============================================================
// Header DNS (12 bytes)
// ============================================================
typedef struct {
    uint16_t id;            // ID da query
    uint16_t flags;         // Flags (QR, Opcode, AA, TC, RD, RA, RCODE)
    uint16_t qdcount;       // Número de queries
    uint16_t ancount;       // Número de respostas
    uint16_t nscount;       // Número de authority records
    uint16_t arcount;       // Número de additional records
} __attribute__((packed)) dns_header_t;

// ============================================================
// Entrada do cache DNS
// ============================================================
typedef struct {
    char      hostname[DNS_MAX_NAME];
    ip_addr_t ip;
    bool      valid;
} dns_cache_entry_t;

// ============================================================
// API pública
// ============================================================

// Inicializa o resolver DNS
void dns_init(void);

// Resolve um hostname para IPv4
// hostname: nome a resolver (ex: "example.com")
// out_ip: endereço IP resultante
// Retorna true se resolveu com sucesso
bool dns_resolve(const char *hostname, ip_addr_t *out_ip);

// Limpa o cache DNS
void dns_cache_clear(void);

// Estatísticas DNS
typedef struct {
    uint32_t queries_sent;
    uint32_t responses_ok;
    uint32_t responses_fail;
    uint32_t cache_hits;
} dns_stats_t;

dns_stats_t dns_get_stats(void);

#endif
