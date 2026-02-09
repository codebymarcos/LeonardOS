// LeonardOS - Socket API
// Wrapper unificado sobre TCP e UDP
//
// Cada socket mapeia internamente para um tcp_conn ou udp bind.
// Abstrai detalhes de handshake, buffers circulares, etc.

#include "socket.h"
#include "tcp.h"
#include "udp.h"
#include "arp.h"
#include "net_config.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"
#include "../drivers/timer/pit.h"

// ============================================================
// Estrutura interna de socket
// ============================================================
typedef struct {
    bool     active;        // Slot em uso
    int      type;          // SOCK_TCP ou SOCK_UDP
    int      conn_id;       // TCP: índice em conns[], UDP: bind index
    ip_addr_t remote_ip;    // Endereço remoto
    uint16_t remote_port;   // Porta remota
    uint16_t local_port;    // Porta local (UDP)
    bool     connected;     // Socket conectado
} socket_entry_t;

static socket_entry_t sockets[SOCKET_MAX];

// Porta UDP ephemeral
static uint16_t udp_next_port = 50000;

// ============================================================
// socket — cria um novo socket
// ============================================================
int socket(int type) {
    if (type != SOCK_TCP && type != SOCK_UDP) return SOCKET_ERROR;

    for (int i = 0; i < SOCKET_MAX; i++) {
        if (!sockets[i].active) {
            kmemset(&sockets[i], 0, sizeof(socket_entry_t));
            sockets[i].active = true;
            sockets[i].type   = type;
            sockets[i].conn_id = -1;
            return i;
        }
    }
    return SOCKET_ERROR; // Sem slots livres
}

// ============================================================
// socket_connect — conecta a um endereço remoto
// ============================================================
int socket_connect(int fd, ip_addr_t dst_ip, uint16_t dst_port, uint32_t timeout_ms) {
    if (fd < 0 || fd >= SOCKET_MAX || !sockets[fd].active) return SOCKET_ERROR;

    socket_entry_t *s = &sockets[fd];
    s->remote_ip   = dst_ip;
    s->remote_port = dst_port;

    if (s->type == SOCK_TCP) {
        // Pre-resolve ARP para gateway
        net_config_t *cfg = net_get_config();
        uint8_t dummy[6];
        arp_resolve(cfg->gateway, dummy);
        pit_sleep_ms(50);
        arp_resolve(cfg->gateway, dummy);

        int conn = tcp_connect(dst_ip, dst_port, timeout_ms);
        if (conn < 0) return SOCKET_ERROR;

        s->conn_id   = conn;
        s->connected = true;
        return 0;

    } else if (s->type == SOCK_UDP) {
        // UDP: associa endereço, aloca porta local
        s->local_port = udp_next_port++;
        if (udp_next_port > 60000) udp_next_port = 50000;
        s->connected = true;
        return 0;
    }

    return SOCKET_ERROR;
}

// ============================================================
// socket_send — envia dados
// ============================================================
int socket_send(int fd, const void *data, uint16_t len) {
    if (fd < 0 || fd >= SOCKET_MAX || !sockets[fd].active || !sockets[fd].connected)
        return SOCKET_ERROR;

    socket_entry_t *s = &sockets[fd];

    if (s->type == SOCK_TCP) {
        return tcp_send(s->conn_id, data, len);

    } else if (s->type == SOCK_UDP) {
        bool ok = udp_send(s->remote_ip, s->remote_port, s->local_port, data, len);
        return ok ? (int)len : SOCKET_ERROR;
    }

    return SOCKET_ERROR;
}

// ============================================================
// socket_recv — recebe dados
// ============================================================
int socket_recv(int fd, void *buf, uint16_t buf_size, uint32_t timeout_ms) {
    if (fd < 0 || fd >= SOCKET_MAX || !sockets[fd].active || !sockets[fd].connected)
        return SOCKET_ERROR;

    socket_entry_t *s = &sockets[fd];

    if (s->type == SOCK_TCP) {
        return tcp_recv(s->conn_id, buf, buf_size, timeout_ms);

    } else if (s->type == SOCK_UDP) {
        uint16_t out_len = 0;
        ip_addr_t src_ip;
        uint16_t src_port;

        bool ok = udp_recv_sync(s->local_port, buf, buf_size,
                                &out_len, &src_ip, &src_port, timeout_ms);
        return ok ? (int)out_len : 0;
    }

    return SOCKET_ERROR;
}

// ============================================================
// socket_close — fecha socket e libera recursos
// ============================================================
void socket_close(int fd) {
    if (fd < 0 || fd >= SOCKET_MAX || !sockets[fd].active) return;

    socket_entry_t *s = &sockets[fd];

    if (s->type == SOCK_TCP && s->conn_id >= 0) {
        tcp_close(s->conn_id);
    } else if (s->type == SOCK_UDP && s->local_port > 0) {
        udp_unbind(s->local_port);
    }

    s->active    = false;
    s->connected = false;
    s->conn_id   = -1;
}

// ============================================================
// Funções auxiliares
// ============================================================

bool socket_is_connected(int fd) {
    if (fd < 0 || fd >= SOCKET_MAX || !sockets[fd].active) return false;
    socket_entry_t *s = &sockets[fd];

    if (s->type == SOCK_TCP) {
        return s->connected && tcp_is_connected(s->conn_id);
    }
    return s->connected;
}

uint16_t socket_available(int fd) {
    if (fd < 0 || fd >= SOCKET_MAX || !sockets[fd].active) return 0;
    socket_entry_t *s = &sockets[fd];

    if (s->type == SOCK_TCP) {
        return tcp_available(s->conn_id);
    }
    return 0; // UDP: sem buffer tracking no socket layer
}

bool socket_peer_closed(int fd) {
    if (fd < 0 || fd >= SOCKET_MAX || !sockets[fd].active) return true;
    socket_entry_t *s = &sockets[fd];

    if (s->type == SOCK_TCP) {
        return tcp_peer_closed(s->conn_id);
    }
    return false; // UDP não tem conceito de peer close
}

// ============================================================
// socket_init
// ============================================================
void socket_init(void) {
    kmemset(sockets, 0, sizeof(sockets));
    udp_next_port = 50000;

    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("Socket: API pronta (TCP/UDP)\n", THEME_BOOT);
}
