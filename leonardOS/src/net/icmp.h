// LeonardOS - ICMP (Internet Control Message Protocol)
// Echo Request/Reply para ping

#ifndef __ICMP_H__
#define __ICMP_H__

#include "../common/types.h"
#include "net_config.h"

// ============================================================
// Tipos ICMP
// ============================================================
#define ICMP_TYPE_ECHO_REPLY     0
#define ICMP_TYPE_DEST_UNREACH   3
#define ICMP_TYPE_ECHO_REQUEST   8
#define ICMP_TYPE_TIME_EXCEEDED  11

// ============================================================
// Header ICMP (8 bytes mínimo)
// ============================================================
typedef struct {
    uint8_t  type;          // Tipo da mensagem
    uint8_t  code;          // Código (subtipo)
    uint16_t checksum;      // Checksum do pacote ICMP completo
    uint16_t identifier;    // ID (para matching request/reply)
    uint16_t sequence;      // Número de sequência
} __attribute__((packed)) icmp_header_t;

// ============================================================
// Estado do ping (para o comando ping acessar)
// ============================================================
#define PING_MAX_PENDING 8

typedef struct {
    bool     active;            // Ping em andamento
    ip_addr_t target;           // IP alvo
    uint16_t identifier;        // ID usado nos requests
    uint16_t seq_sent;          // Próximo seq a enviar
    uint16_t seq_received;      // Quantos replies recebidos

    // Tracking de respostas
    volatile bool reply_received;   // Flag: último reply chegou
    uint16_t last_reply_seq;        // Seq do último reply
    uint32_t last_reply_ttl;        // TTL do reply
} ping_state_t;

// ============================================================
// API pública
// ============================================================

// Inicializa o módulo ICMP (registra handler no IPv4)
void icmp_init(void);

// Envia um ICMP Echo Request
// dst_ip: IP destino
// identifier: ID do ping (para distinguir sessões)
// sequence: número de sequência
bool icmp_send_echo_request(ip_addr_t dst_ip, uint16_t identifier,
                            uint16_t sequence);

// Acesso ao estado do ping (para cmd_ping)
ping_state_t *icmp_get_ping_state(void);

// Reset do estado do ping
void icmp_reset_ping(void);

// Estatísticas ICMP
typedef struct {
    uint32_t echo_requests_sent;
    uint32_t echo_requests_received;
    uint32_t echo_replies_sent;
    uint32_t echo_replies_received;
} icmp_stats_t;

icmp_stats_t icmp_get_stats(void);

#endif
