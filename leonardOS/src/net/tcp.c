// LeonardOS - TCP (Transmission Control Protocol)
// Implementação simplificada: client-only, sem retransmissão,
// suficiente para HTTP/1.0 via QEMU user networking

#include "tcp.h"
#include "ipv4.h"
#include "ethernet.h"
#include "net_config.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../drivers/timer/pit.h"

// ============================================================
// Conexões ativas
// ============================================================
static tcp_conn_t conns[TCP_MAX_CONNS];
static tcp_stats_t stats;

// Porta local ephemeral (começa em 49152)
static uint16_t next_local_port = 49152;

tcp_stats_t tcp_get_stats(void) {
    return stats;
}

// ============================================================
// ISN (Initial Sequence Number) — usa PIT ticks para variação
// ============================================================
static uint32_t isn_counter = 0x1000;

static uint32_t tcp_generate_isn(void) {
    isn_counter += 64000 + pit_get_ticks(); // Variação baseada no timer
    return isn_counter;
}

// ============================================================
// Delay preciso via PIT timer
// ============================================================
static void tcp_delay_ms(uint32_t ms) {
    pit_sleep_ms(ms);
}

// ============================================================
// Aloca porta local ephemeral
// ============================================================
static uint16_t tcp_alloc_port(void) {
    uint16_t port = next_local_port++;
    if (next_local_port > 60000) next_local_port = 49152;
    return port;
}

// ============================================================
// tcp_send_segment — envia um segmento TCP
// ============================================================
static bool tcp_send_segment(tcp_conn_t *conn, uint8_t flags,
                             const void *data, uint16_t data_len) {
    static uint8_t tcp_buf[ETH_MTU];

    tcp_header_t *hdr = (tcp_header_t *)tcp_buf;

    hdr->src_port    = htons(conn->local_port);
    hdr->dst_port    = htons(conn->remote_port);
    hdr->seq_num     = htonl(conn->seq_next);
    hdr->ack_num     = htonl(conn->ack_next);
    hdr->data_offset = (TCP_HLEN_MIN / 4) << 4; // 5 words, no options
    hdr->flags       = flags;
    hdr->window      = htons(TCP_WINDOW);
    hdr->checksum    = 0;
    hdr->urgent_ptr  = 0;

    // Copia dados
    if (data && data_len > 0) {
        kmemcpy(tcp_buf + TCP_HLEN_MIN, data, data_len);
    }

    uint16_t tcp_total = TCP_HLEN_MIN + data_len;

    // Calcula checksum com pseudo-header
    {
        net_config_t *cfg = net_get_config();
        static uint8_t cksum_buf[ETH_MTU + 12];
        tcp_pseudo_header_t *pseudo = (tcp_pseudo_header_t *)cksum_buf;

        kmemcpy(pseudo->src_ip, cfg->ip.octets, 4);
        kmemcpy(pseudo->dst_ip, conn->remote_ip.octets, 4);
        pseudo->zero       = 0;
        pseudo->protocol   = IP_PROTO_TCP;
        pseudo->tcp_length = htons(tcp_total);

        kmemcpy(cksum_buf + sizeof(tcp_pseudo_header_t), tcp_buf, tcp_total);

        uint16_t cksum = ip_checksum(cksum_buf,
                                     sizeof(tcp_pseudo_header_t) + tcp_total);
        hdr->checksum = cksum;
    }

    bool ok = ipv4_send(conn->remote_ip, IP_PROTO_TCP, tcp_buf, tcp_total);
    if (ok) {
        stats.segments_tx++;

        // Atualiza seq para dados enviados
        if (data_len > 0) conn->seq_next += data_len;
        if (flags & TCP_SYN) conn->seq_next++;  // SYN consome 1 seq
        if (flags & TCP_FIN) conn->seq_next++;  // FIN consome 1 seq
    }
    return ok;
}

// ============================================================
// Encontra conexão por porta local + IP/porta remota
// ============================================================
static tcp_conn_t *tcp_find_conn(uint16_t local_port, ip_addr_t remote_ip,
                                 uint16_t remote_port) {
    for (int i = 0; i < TCP_MAX_CONNS; i++) {
        if (conns[i].active &&
            conns[i].local_port == local_port &&
            conns[i].remote_port == remote_port &&
            ip_equal(conns[i].remote_ip, remote_ip)) {
            return &conns[i];
        }
    }
    return 0;
}

// ============================================================
// tcp_rx_handler — recebe segmentos TCP do IPv4
// ============================================================
static void tcp_rx_handler(const void *payload, uint16_t len,
                           ip_addr_t src_ip) {
    if (len < TCP_HLEN_MIN) return;

    const tcp_header_t *hdr = (const tcp_header_t *)payload;

    uint16_t dst_port = ntohs(hdr->dst_port);
    uint16_t src_port = ntohs(hdr->src_port);
    uint8_t  data_off = (hdr->data_offset >> 4) * 4;

    if (data_off < TCP_HLEN_MIN || data_off > len) return;

    stats.segments_rx++;

    // Encontra conexão
    tcp_conn_t *conn = tcp_find_conn(dst_port, src_ip, src_port);
    if (!conn) {
        // Nenhuma conexão — envia RST se não for RST
        if (!(hdr->flags & TCP_RST)) {
            // Cria conexão temporária para enviar RST
            // (simplificação: ignora pacotes sem conexão)
        }
        return;
    }

    uint32_t seg_seq = ntohl(hdr->seq_num);
    uint32_t seg_ack = ntohl(hdr->ack_num);
    uint16_t data_len = len - data_off;
    const uint8_t *data = (const uint8_t *)payload + data_off;

    // RST recebido — fecha conexão
    if (hdr->flags & TCP_RST) {
        conn->rst_received = true;
        conn->state = TCP_STATE_CLOSED;
        stats.resets++;
        return;
    }

    switch (conn->state) {
        case TCP_STATE_SYN_SENT:
            // Esperando SYN-ACK
            if ((hdr->flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                conn->ack_next = seg_seq + 1;  // SYN consome 1 seq

                // Verifica se ACK confirma nosso SYN
                if (seg_ack == conn->initial_seq + 1) {
                    // Envia ACK para completar handshake
                    conn->state = TCP_STATE_ESTABLISHED;
                    conn->syn_ack_received = true;
                    tcp_send_segment(conn, TCP_ACK, 0, 0);
                    stats.handshake_ok++;
                }
            }
            break;

        case TCP_STATE_ESTABLISHED:
            // ACK para nossos dados
            if (hdr->flags & TCP_ACK) {
                // seg_ack confirma bytes até seg_ack - 1
                // (simplificação: sem tracking de retransmissão)
                (void)seg_ack;
            }

            // Dados recebidos
            if (data_len > 0) {
                // Verifica se é o seq esperado
                if (seg_seq == conn->ack_next) {
                    // Copia para buffer circular
                    for (uint16_t i = 0; i < data_len; i++) {
                        if (conn->rx_count < TCP_RX_BUF_SIZE) {
                            conn->rx_buf[conn->rx_write] = data[i];
                            conn->rx_write = (conn->rx_write + 1) % TCP_RX_BUF_SIZE;
                            conn->rx_count++;
                        }
                    }
                    conn->ack_next += data_len;
                    conn->data_available = true;

                    // Envia ACK
                    tcp_send_segment(conn, TCP_ACK, 0, 0);
                }
                // Se seq fora de ordem: reenvia ACK do último confirmado
                else {
                    tcp_send_segment(conn, TCP_ACK, 0, 0);
                }
            }

            // FIN recebido
            if (hdr->flags & TCP_FIN) {
                conn->ack_next++;  // FIN consome 1 seq
                conn->fin_received = true;
                conn->state = TCP_STATE_CLOSE_WAIT;
                // Envia ACK do FIN
                tcp_send_segment(conn, TCP_ACK, 0, 0);
            }
            break;

        case TCP_STATE_FIN_WAIT_1:
            // Esperando ACK do nosso FIN
            if (hdr->flags & TCP_ACK) {
                // Dados que ainda podem chegar
                if (data_len > 0) {
                    if (seg_seq == conn->ack_next) {
                        for (uint16_t i = 0; i < data_len; i++) {
                            if (conn->rx_count < TCP_RX_BUF_SIZE) {
                                conn->rx_buf[conn->rx_write] = data[i];
                                conn->rx_write = (conn->rx_write + 1) % TCP_RX_BUF_SIZE;
                                conn->rx_count++;
                            }
                        }
                        conn->ack_next += data_len;
                        conn->data_available = true;
                    }
                }

                if (hdr->flags & TCP_FIN) {
                    // FIN-ACK simultâneo
                    conn->ack_next++;
                    conn->fin_received = true;
                    conn->state = TCP_STATE_TIME_WAIT;
                    tcp_send_segment(conn, TCP_ACK, 0, 0);
                } else {
                    conn->state = TCP_STATE_FIN_WAIT_2;
                }
            }
            break;

        case TCP_STATE_FIN_WAIT_2:
            // Dados que ainda podem chegar antes do FIN
            if (data_len > 0 && seg_seq == conn->ack_next) {
                for (uint16_t i = 0; i < data_len; i++) {
                    if (conn->rx_count < TCP_RX_BUF_SIZE) {
                        conn->rx_buf[conn->rx_write] = data[i];
                        conn->rx_write = (conn->rx_write + 1) % TCP_RX_BUF_SIZE;
                        conn->rx_count++;
                    }
                }
                conn->ack_next += data_len;
                conn->data_available = true;
                tcp_send_segment(conn, TCP_ACK, 0, 0);
            }

            // Esperando FIN do remoto
            if (hdr->flags & TCP_FIN) {
                conn->ack_next++;
                conn->fin_received = true;
                conn->state = TCP_STATE_TIME_WAIT;
                tcp_send_segment(conn, TCP_ACK, 0, 0);
            }
            break;

        case TCP_STATE_LAST_ACK:
            if (hdr->flags & TCP_ACK) {
                conn->state = TCP_STATE_CLOSED;
                conn->active = false;
            }
            break;

        default:
            break;
    }
}

// ============================================================
// tcp_connect — 3-way handshake com servidor remoto
// ============================================================
int tcp_connect(ip_addr_t dst_ip, uint16_t dst_port, uint32_t timeout_ms) {
    // Encontra slot livre
    int slot = -1;
    for (int i = 0; i < TCP_MAX_CONNS; i++) {
        if (!conns[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    tcp_conn_t *conn = &conns[slot];
    kmemset(conn, 0, sizeof(tcp_conn_t));

    conn->active      = true;
    conn->state       = TCP_STATE_SYN_SENT;
    conn->remote_ip   = dst_ip;
    conn->remote_port = dst_port;
    conn->local_port  = tcp_alloc_port();
    conn->initial_seq = tcp_generate_isn();
    conn->seq_next    = conn->initial_seq;
    conn->ack_next    = 0;

    stats.connections++;

    // Envia SYN
    if (!tcp_send_segment(conn, TCP_SYN, 0, 0)) {
        conn->active = false;
        conn->state  = TCP_STATE_CLOSED;
        stats.handshake_fail++;
        return -1;
    }

    // Espera SYN-ACK com timeout
    for (uint32_t elapsed = 0; elapsed < timeout_ms; elapsed += 10) {
        if (conn->syn_ack_received) {
            return slot;
        }
        if (conn->rst_received) {
            conn->active = false;
            conn->state  = TCP_STATE_CLOSED;
            stats.handshake_fail++;
            return -1;
        }
        tcp_delay_ms(10);
    }

    // Timeout — tenta mais uma vez com SYN retry
    conn->seq_next = conn->initial_seq; // Reset seq
    tcp_send_segment(conn, TCP_SYN, 0, 0);

    for (uint32_t elapsed = 0; elapsed < timeout_ms; elapsed += 10) {
        if (conn->syn_ack_received) {
            return slot;
        }
        if (conn->rst_received) {
            break;
        }
        tcp_delay_ms(10);
    }

    // Falhou
    conn->active = false;
    conn->state  = TCP_STATE_CLOSED;
    stats.handshake_fail++;
    return -1;
}

// ============================================================
// tcp_send — envia dados por conexão estabelecida
// ============================================================
int tcp_send(int conn_id, const void *data, uint16_t len) {
    if (conn_id < 0 || conn_id >= TCP_MAX_CONNS) return -1;

    tcp_conn_t *conn = &conns[conn_id];
    if (!conn->active || conn->state != TCP_STATE_ESTABLISHED) return -1;

    // Fragmenta em segmentos de TCP_MSS
    const uint8_t *ptr = (const uint8_t *)data;
    uint16_t remaining = len;
    uint16_t total_sent = 0;

    while (remaining > 0) {
        uint16_t seg_len = remaining;
        if (seg_len > TCP_MSS) seg_len = TCP_MSS;

        uint8_t flags = TCP_ACK;
        if (remaining <= TCP_MSS) flags |= TCP_PSH; // Push no último

        if (!tcp_send_segment(conn, flags, ptr, seg_len)) {
            return (total_sent > 0) ? (int)total_sent : -1;
        }

        ptr        += seg_len;
        remaining  -= seg_len;
        total_sent += seg_len;
    }

    return (int)total_sent;
}

// ============================================================
// tcp_recv — recebe dados com polling/timeout
// ============================================================
int tcp_recv(int conn_id, void *buf, uint16_t buf_size, uint32_t timeout_ms) {
    if (conn_id < 0 || conn_id >= TCP_MAX_CONNS) return -1;

    tcp_conn_t *conn = &conns[conn_id];
    if (!conn->active) return -1;

    // Se RST recebido
    if (conn->rst_received) return -1;

    // Polling: espera dados ou timeout
    for (uint32_t elapsed = 0; elapsed < timeout_ms; elapsed += 5) {
        if (conn->rx_count > 0) {
            // Copia dados do buffer circular
            uint16_t to_read = conn->rx_count;
            if (to_read > buf_size) to_read = buf_size;

            uint8_t *dst = (uint8_t *)buf;
            for (uint16_t i = 0; i < to_read; i++) {
                dst[i] = conn->rx_buf[conn->rx_read];
                conn->rx_read = (conn->rx_read + 1) % TCP_RX_BUF_SIZE;
            }
            conn->rx_count -= to_read;
            conn->data_available = (conn->rx_count > 0);

            return (int)to_read;
        }

        // Conexão fechada pelo peer e sem dados restantes
        if (conn->fin_received && conn->rx_count == 0) {
            return 0; // EOF
        }

        if (conn->rst_received) return -1;

        // Sleep ~5ms usando PIT
        pit_sleep_ms(5);
    }

    // Timeout — verifica dados uma última vez
    if (conn->rx_count > 0) {
        uint16_t to_read = conn->rx_count;
        if (to_read > buf_size) to_read = buf_size;

        uint8_t *dst = (uint8_t *)buf;
        for (uint16_t i = 0; i < to_read; i++) {
            dst[i] = conn->rx_buf[conn->rx_read];
            conn->rx_read = (conn->rx_read + 1) % TCP_RX_BUF_SIZE;
        }
        conn->rx_count -= to_read;
        return (int)to_read;
    }

    // FIN sem dados
    if (conn->fin_received) return 0;

    return 0; // Timeout sem dados
}

// ============================================================
// tcp_close — fecha conexão (envia FIN)
// ============================================================
void tcp_close(int conn_id) {
    if (conn_id < 0 || conn_id >= TCP_MAX_CONNS) return;

    tcp_conn_t *conn = &conns[conn_id];
    if (!conn->active) return;

    if (conn->state == TCP_STATE_ESTABLISHED) {
        conn->state = TCP_STATE_FIN_WAIT_1;
        tcp_send_segment(conn, TCP_FIN | TCP_ACK, 0, 0);

        // Espera ACK do FIN com timeout curto (2s)
        for (uint32_t elapsed = 0; elapsed < 2000; elapsed += 10) {
            if (conn->state == TCP_STATE_TIME_WAIT ||
                conn->state == TCP_STATE_CLOSED ||
                conn->rst_received) {
                break;
            }
            // Continua processando dados que chegam durante o close
            if (conn->state == TCP_STATE_FIN_WAIT_2 && conn->fin_received) {
                break;
            }
            tcp_delay_ms(10);
        }
    } else if (conn->state == TCP_STATE_CLOSE_WAIT) {
        conn->state = TCP_STATE_LAST_ACK;
        tcp_send_segment(conn, TCP_FIN | TCP_ACK, 0, 0);

        // Espera ACK
        for (uint32_t elapsed = 0; elapsed < 1000; elapsed += 10) {
            if (conn->state == TCP_STATE_CLOSED || conn->rst_received) break;
            tcp_delay_ms(10);
        }
    }

    // Libera slot
    conn->active = false;
    conn->state  = TCP_STATE_CLOSED;
}

// ============================================================
// tcp_is_connected
// ============================================================
bool tcp_is_connected(int conn_id) {
    if (conn_id < 0 || conn_id >= TCP_MAX_CONNS) return false;
    return conns[conn_id].active &&
           conns[conn_id].state == TCP_STATE_ESTABLISHED;
}

// ============================================================
// tcp_available — bytes disponíveis para leitura
// ============================================================
uint16_t tcp_available(int conn_id) {
    if (conn_id < 0 || conn_id >= TCP_MAX_CONNS) return 0;
    return conns[conn_id].rx_count;
}

// ============================================================
// tcp_peer_closed — verifica se peer fechou e buffer vazio
// ============================================================
bool tcp_peer_closed(int conn_id) {
    if (conn_id < 0 || conn_id >= TCP_MAX_CONNS) return true;
    tcp_conn_t *conn = &conns[conn_id];
    return conn->fin_received && conn->rx_count == 0;
}

// ============================================================
// tcp_init — registra handler TCP no IPv4
// ============================================================
void tcp_init(void) {
    kmemset(&stats, 0, sizeof(stats));
    kmemset(conns, 0, sizeof(conns));
    next_local_port = 49152;
    isn_counter = 0x1000;

    ipv4_register_handler(IP_PROTO_TCP, tcp_rx_handler);

    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("TCP: protocolo registrado\n", THEME_BOOT);
}
