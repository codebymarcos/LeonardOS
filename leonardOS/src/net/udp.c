// LeonardOS - UDP (User Datagram Protocol)
// Envia/recebe datagramas, binding por porta, checksum com pseudo-header

#include "udp.h"
#include "ipv4.h"
#include "ethernet.h"
#include "net_config.h"
#include "../common/string.h"
#include "../drivers/vga/vga.h"
#include "../common/colors.h"

// ============================================================
// Tabela de sockets (bind por porta)
// ============================================================
static udp_socket_t sockets[UDP_MAX_SOCKETS];
static udp_stats_t stats;

// Estado de recepção síncrona
static udp_recv_state_t recv_state;
static uint16_t recv_sync_port = 0;
static volatile bool recv_sync_active = false;

udp_stats_t udp_get_stats(void) {
    return stats;
}

// ============================================================
// udp_bind — registra callback para uma porta
// ============================================================
bool udp_bind(uint16_t port, udp_rx_callback_t callback) {
    // Verifica se já existe
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (sockets[i].active && sockets[i].port == port) {
            return false; // Porta já em uso
        }
    }

    // Encontra slot livre
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (!sockets[i].active) {
            sockets[i].port     = port;
            sockets[i].callback = callback;
            sockets[i].active   = true;
            return true;
        }
    }
    return false; // Sem slots livres
}

// ============================================================
// udp_unbind — remove binding de uma porta
// ============================================================
void udp_unbind(uint16_t port) {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (sockets[i].active && sockets[i].port == port) {
            sockets[i].active = false;
            sockets[i].callback = 0;
            return;
        }
    }
}

// ============================================================
// Callback interno para recepção síncrona
// ============================================================
static void udp_sync_callback(const void *data, uint16_t data_len,
                               ip_addr_t src_ip, uint16_t src_port) {
    if (!recv_sync_active) return;

    uint16_t copy_len = data_len;
    if (copy_len > UDP_RECV_BUF_SIZE) copy_len = UDP_RECV_BUF_SIZE;

    kmemcpy(recv_state.data, data, copy_len);
    recv_state.data_len = copy_len;
    recv_state.src_ip   = src_ip;
    recv_state.src_port = src_port;
    recv_state.ready    = true;
}

// ============================================================
// udp_recv_sync — recepção síncrona com timeout (busy-wait)
// ============================================================
bool udp_recv_sync(uint16_t port, void *buf, uint16_t buf_size,
                   uint16_t *out_len, ip_addr_t *out_src_ip,
                   uint16_t *out_src_port, uint32_t timeout_ms) {

    // Bind temporário para recepção
    recv_state.ready = false;
    recv_sync_port = port;
    recv_sync_active = true;

    // Registra callback síncrono (substitui se já existe)
    udp_unbind(port);
    udp_bind(port, udp_sync_callback);

    // Polling com busy-wait
    for (uint32_t elapsed = 0; elapsed < timeout_ms; elapsed += 5) {
        if (recv_state.ready) {
            // Dados recebidos!
            uint16_t copy_len = recv_state.data_len;
            if (copy_len > buf_size) copy_len = buf_size;

            kmemcpy(buf, recv_state.data, copy_len);
            if (out_len) *out_len = copy_len;
            if (out_src_ip) *out_src_ip = recv_state.src_ip;
            if (out_src_port) *out_src_port = recv_state.src_port;

            // Cleanup
            recv_sync_active = false;
            udp_unbind(port);
            return true;
        }

        // Busy-wait ~5ms
        for (volatile uint32_t j = 0; j < 25000; j++);
    }

    // Timeout
    recv_sync_active = false;
    udp_unbind(port);
    return false;
}

// ============================================================
// udp_send — envia datagrama UDP via IPv4
// ============================================================
bool udp_send(ip_addr_t dst_ip, uint16_t dst_port, uint16_t src_port,
              const void *data, uint16_t data_len) {

    // Limite: header UDP (8) + dados deve caber no payload IPv4
    if (data_len > ETH_MTU - IPV4_HLEN - sizeof(udp_header_t)) {
        stats.tx_errors++;
        return false;
    }

    // Monta pacote UDP
    static uint8_t udp_buf[ETH_MTU];
    udp_header_t *hdr = (udp_header_t *)udp_buf;

    uint16_t udp_total = sizeof(udp_header_t) + data_len;

    hdr->src_port = htons(src_port);
    hdr->dst_port = htons(dst_port);
    hdr->length   = htons(udp_total);
    hdr->checksum = 0; // Checksum opcional em UDP sobre IPv4

    // Copia dados
    kmemcpy(udp_buf + sizeof(udp_header_t), data, data_len);

    // Calcula checksum UDP com pseudo-header
    // (essencial para confiabilidade, embora tecnicamente opcional em IPv4)
    {
        net_config_t *cfg = net_get_config();

        // Buffer para pseudo-header + pacote UDP
        static uint8_t cksum_buf[ETH_MTU + 12];
        udp_pseudo_header_t *pseudo = (udp_pseudo_header_t *)cksum_buf;

        kmemcpy(pseudo->src_ip, cfg->ip.octets, 4);
        kmemcpy(pseudo->dst_ip, dst_ip.octets, 4);
        pseudo->zero       = 0;
        pseudo->protocol   = IP_PROTO_UDP;
        pseudo->udp_length = htons(udp_total);

        // Copia pacote UDP após pseudo-header
        kmemcpy(cksum_buf + sizeof(udp_pseudo_header_t), udp_buf, udp_total);

        uint16_t cksum_len = sizeof(udp_pseudo_header_t) + udp_total;
        uint16_t cksum = ip_checksum(cksum_buf, cksum_len);
        if (cksum == 0) cksum = 0xFFFF; // RFC 768: 0 = no checksum
        hdr->checksum = cksum;
    }

    // Envia via IPv4
    bool ok = ipv4_send(dst_ip, IP_PROTO_UDP, udp_buf, udp_total);
    if (ok) {
        stats.datagrams_tx++;
    } else {
        stats.tx_errors++;
    }
    return ok;
}

// ============================================================
// udp_rx_handler — recebe datagramas do IPv4
// ============================================================
static void udp_rx_handler(const void *payload, uint16_t len,
                           ip_addr_t src_ip) {
    if (len < sizeof(udp_header_t)) return;

    const udp_header_t *hdr = (const udp_header_t *)payload;

    uint16_t dst_port = ntohs(hdr->dst_port);
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t udp_len  = ntohs(hdr->length);

    // Validação básica
    if (udp_len < sizeof(udp_header_t) || udp_len > len) return;

    uint16_t data_len = udp_len - sizeof(udp_header_t);
    const uint8_t *data = (const uint8_t *)payload + sizeof(udp_header_t);

    stats.datagrams_rx++;

    // Procura socket bound nessa porta
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
        if (sockets[i].active && sockets[i].port == dst_port) {
            if (sockets[i].callback) {
                sockets[i].callback(data, data_len, src_ip, src_port);
            }
            return;
        }
    }

    // Nenhum socket — descarta
    stats.rx_no_socket++;
}

// ============================================================
// udp_init — registra handler UDP no IPv4
// ============================================================
void udp_init(void) {
    kmemset(&stats, 0, sizeof(stats));
    kmemset(sockets, 0, sizeof(sockets));
    kmemset(&recv_state, 0, sizeof(recv_state));
    recv_sync_active = false;

    ipv4_register_handler(IP_PROTO_UDP, udp_rx_handler);

    vga_puts_color("[OK] ", THEME_BOOT_OK);
    vga_puts_color("UDP: protocolo registrado\n", THEME_BOOT);
}
