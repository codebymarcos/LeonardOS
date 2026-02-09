// LeonardOS - UDP (User Datagram Protocol)
// Envia e recebe datagramas UDP, binding por porta

#ifndef __UDP_H__
#define __UDP_H__

#include "../common/types.h"
#include "net_config.h"

// ============================================================
// Header UDP (8 bytes)
// ============================================================
typedef struct {
    uint16_t src_port;      // Porta origem (network byte order)
    uint16_t dst_port;      // Porta destino (network byte order)
    uint16_t length;        // Tamanho total (header + dados)
    uint16_t checksum;      // Checksum (pode ser 0 = desabilitado)
} __attribute__((packed)) udp_header_t;

// ============================================================
// Pseudo-header para cálculo de checksum UDP
// ============================================================
typedef struct {
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
    uint8_t  zero;
    uint8_t  protocol;      // 17 = UDP
    uint16_t udp_length;
} __attribute__((packed)) udp_pseudo_header_t;

// ============================================================
// Callback para recepção UDP
// Chamado quando chega um datagrama na porta bound
// ============================================================
typedef void (*udp_rx_callback_t)(const void *data, uint16_t data_len,
                                  ip_addr_t src_ip, uint16_t src_port);

// ============================================================
// Socket UDP (bind de porta)
// ============================================================
#define UDP_MAX_SOCKETS 16

typedef struct {
    uint16_t        port;       // Porta local (host byte order)
    udp_rx_callback_t callback; // Callback de recepção
    bool            active;     // Slot em uso
} udp_socket_t;

// ============================================================
// Recepção síncrona (polling) — para DNS, etc.
// ============================================================
#define UDP_RECV_BUF_SIZE 512

typedef struct {
    uint8_t   data[UDP_RECV_BUF_SIZE];
    uint16_t  data_len;
    ip_addr_t src_ip;
    uint16_t  src_port;
    volatile bool ready;    // Flag: dados prontos
} udp_recv_state_t;

// ============================================================
// API pública
// ============================================================

// Inicializa camada UDP (registra handler no IPv4)
void udp_init(void);

// Envia um datagrama UDP
// dst_ip: IP destino
// dst_port: porta destino (host byte order)
// src_port: porta origem (host byte order)
// data: dados a enviar
// data_len: tamanho dos dados
// Retorna true se enviado com sucesso
bool udp_send(ip_addr_t dst_ip, uint16_t dst_port, uint16_t src_port,
              const void *data, uint16_t data_len);

// Bind: registra callback para uma porta UDP
// Retorna true se registrado com sucesso
bool udp_bind(uint16_t port, udp_rx_callback_t callback);

// Unbind: remove callback de uma porta
void udp_unbind(uint16_t port);

// Recepção síncrona: espera um datagrama em uma porta com timeout
// Útil para DNS e outros protocolos request/reply
// timeout_ms: timeout em ms (busy-wait)
// Retorna true se dados recebidos
bool udp_recv_sync(uint16_t port, void *buf, uint16_t buf_size,
                   uint16_t *out_len, ip_addr_t *out_src_ip,
                   uint16_t *out_src_port, uint32_t timeout_ms);

// Estatísticas UDP
typedef struct {
    uint32_t datagrams_rx;
    uint32_t datagrams_tx;
    uint32_t rx_no_socket;      // Sem socket bound na porta
    uint32_t rx_bad_checksum;
    uint32_t tx_errors;
} udp_stats_t;

udp_stats_t udp_get_stats(void);

#endif
