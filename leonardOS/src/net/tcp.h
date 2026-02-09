// LeonardOS - TCP (Transmission Control Protocol)
// Conexões TCP simplificadas para HTTP client
// Estado: CLOSED → SYN_SENT → ESTABLISHED → FIN_WAIT → CLOSED

#ifndef __TCP_H__
#define __TCP_H__

#include "../common/types.h"
#include "net_config.h"

// ============================================================
// Constantes TCP
// ============================================================
#define TCP_HLEN_MIN    20      // Header mínimo (sem opções)
#define TCP_WINDOW      16384   // Window size (increased for performance)
#define TCP_MSS         1460    // Maximum Segment Size (ETH_MTU - IP - TCP)
#define TCP_MAX_CONNS   4       // Máximo de conexões simultâneas

// Flags TCP
#define TCP_FIN         0x01
#define TCP_SYN         0x02
#define TCP_RST         0x04
#define TCP_PSH         0x08
#define TCP_ACK         0x10
#define TCP_URG         0x20

// ============================================================
// Header TCP (20 bytes mínimo)
// ============================================================
typedef struct {
    uint16_t src_port;          // Porta origem
    uint16_t dst_port;          // Porta destino
    uint32_t seq_num;           // Número de sequência
    uint32_t ack_num;           // Número de acknowledgment
    uint8_t  data_offset;       // Data offset (4 bits) + Reserved (4 bits)
    uint8_t  flags;             // Flags (6 bits de controle)
    uint16_t window;            // Window size
    uint16_t checksum;          // Checksum
    uint16_t urgent_ptr;        // Urgent pointer
} __attribute__((packed)) tcp_header_t;

// ============================================================
// Pseudo-header TCP (para checksum)
// ============================================================
typedef struct {
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
    uint8_t  zero;
    uint8_t  protocol;          // 6 = TCP
    uint16_t tcp_length;
} __attribute__((packed)) tcp_pseudo_header_t;

// ============================================================
// Estados TCP (simplificado)
// ============================================================
typedef enum {
    TCP_STATE_CLOSED,
    TCP_STATE_SYN_SENT,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT
} tcp_state_t;

// ============================================================
// Conexão TCP
// ============================================================
#define TCP_RX_BUF_SIZE 32768   // Buffer de recepção circular (32KB)

typedef struct {
    bool        active;         // Slot em uso
    tcp_state_t state;          // Estado da conexão

    // Endpoints
    ip_addr_t   remote_ip;
    uint16_t    remote_port;
    uint16_t    local_port;

    // Sequência
    uint32_t    seq_next;       // Próximo seq a enviar
    uint32_t    ack_next;       // Próximo ack esperado (seq remoto)
    uint32_t    initial_seq;    // ISN local

    // Buffer de recepção
    uint8_t     rx_buf[TCP_RX_BUF_SIZE];
    uint16_t    rx_write;       // Posição de escrita
    uint16_t    rx_read;        // Posição de leitura
    uint16_t    rx_count;       // Bytes no buffer

    // Flags de controle
    volatile bool syn_ack_received;  // Handshake: SYN-ACK recebido
    volatile bool fin_received;      // FIN recebido do remoto
    volatile bool rst_received;      // RST recebido
    volatile bool data_available;    // Novos dados no buffer
} tcp_conn_t;

// ============================================================
// API pública
// ============================================================

// Inicializa camada TCP (registra handler no IPv4)
void tcp_init(void);

// Conecta a um servidor remoto (3-way handshake)
// Retorna índice da conexão (0..3) ou -1 se falhou
// timeout_ms: tempo máximo para handshake
int tcp_connect(ip_addr_t dst_ip, uint16_t dst_port, uint32_t timeout_ms);

// Envia dados por uma conexão TCP
// conn_id: índice retornado por tcp_connect
// Retorna bytes enviados ou -1 se erro
int tcp_send(int conn_id, const void *data, uint16_t len);

// Recebe dados de uma conexão TCP (polling com timeout)
// Retorna bytes lidos, 0 se timeout, -1 se erro/conexão fechada
int tcp_recv(int conn_id, void *buf, uint16_t buf_size, uint32_t timeout_ms);

// Fecha uma conexão TCP (envia FIN)
void tcp_close(int conn_id);

// Verifica se a conexão está ativa
bool tcp_is_connected(int conn_id);

// Verifica se há dados disponíveis sem bloquear
uint16_t tcp_available(int conn_id);

// Verifica se o peer já fechou (FIN recebido) e todos os dados foram lidos
bool tcp_peer_closed(int conn_id);

// Estatísticas TCP
typedef struct {
    uint32_t segments_rx;
    uint32_t segments_tx;
    uint32_t connections;
    uint32_t handshake_ok;
    uint32_t handshake_fail;
    uint32_t resets;
    uint32_t rx_bad_checksum;
} tcp_stats_t;

tcp_stats_t tcp_get_stats(void);

#endif
