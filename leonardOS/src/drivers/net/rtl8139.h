// LeonardOS - RTL8139 Network Driver
// Driver para placa de rede Realtek RTL8139 (PCI, I/O mapped)

#ifndef __RTL8139_H__
#define __RTL8139_H__

#include "../../common/types.h"

// Tamanho máximo de um frame Ethernet (sem CRC, hardware calcula)
#define ETH_FRAME_MAX  1536
#define ETH_ALEN       6       // Tamanho de endereço MAC

// Estatísticas da NIC
typedef struct {
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
    uint32_t tx_errors;
    uint32_t rx_errors;
} nic_stats_t;

// ============================================================
// API pública
// ============================================================

// Inicializa o RTL8139 (scan PCI, reset, configura RX/TX, registra IRQ)
// Retorna true se a NIC foi encontrada e inicializada
bool rtl8139_init(void);

// Envia um pacote raw (frame Ethernet completo)
// data: ponteiro para o frame, len: tamanho total
// Retorna true se enfileirado com sucesso
bool rtl8139_send(const void *data, uint16_t len);

// Obtém o endereço MAC da NIC (copia 6 bytes para mac_out)
void rtl8139_get_mac(uint8_t *mac_out);

// Verifica se a NIC está presente e inicializada
bool rtl8139_is_present(void);

// Retorna estatísticas da NIC
nic_stats_t rtl8139_get_stats(void);

// Callback para pacotes recebidos
// Registra função chamada pelo IRQ handler quando um pacote chega
typedef void (*rtl8139_rx_callback_t)(const void *data, uint16_t len);
void rtl8139_set_rx_callback(rtl8139_rx_callback_t cb);

#endif
