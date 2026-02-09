// LeonardOS - RTL8139 Network Driver
// Implementação do driver Realtek RTL8139
//
// O RTL8139 é uma NIC PCI simples que opera via I/O ports:
//   - 4 TX descriptors (round-robin)
//   - 1 RX ring buffer contínuo (8KB + 16 + 1500 wrap)
//   - IRQ para TX ok, RX ok, erros
//
// Referência: RTL8139 Programming Guide + OSDev wiki

#include "rtl8139.h"
#include "../pci/pci.h"
#include "../../common/io.h"
#include "../../common/string.h"
#include "../../common/types.h"
#include "../../cpu/isr.h"
#include "../../drivers/pic/pic.h"
#include "../../drivers/vga/vga.h"
#include "../../common/colors.h"
#include "../../memory/pmm.h"

// ============================================================
// RTL8139 PCI identifiers
// ============================================================
#define RTL8139_VENDOR_ID   0x10EC
#define RTL8139_DEVICE_ID   0x8139

// ============================================================
// RTL8139 Register offsets (I/O mapped)
// ============================================================
#define REG_MAC0        0x00    // MAC address bytes 0-3 (32-bit read)
#define REG_MAC4        0x04    // MAC address bytes 4-5 (16-bit read)
#define REG_TX_STATUS0  0x10    // TX status descriptor 0 (32-bit)
#define REG_TX_ADDR0    0x20    // TX buffer address 0 (32-bit, physical addr)
#define REG_RX_BUF      0x30    // RX buffer physical address (32-bit)
#define REG_CMD         0x37    // Command register (8-bit)
#define REG_CAPR        0x38    // Current Address of Packet Read (16-bit)
#define REG_CBR         0x3A    // Current Buffer address (16-bit, read-only)
#define REG_IMR         0x3C    // Interrupt Mask Register (16-bit)
#define REG_ISR         0x3E    // Interrupt Status Register (16-bit)
#define REG_TX_CONFIG   0x40    // TX configuration (32-bit)
#define REG_RX_CONFIG   0x44    // RX configuration (32-bit)
#define REG_CONFIG1     0x52    // Configuration 1 (8-bit)

// Command bits
#define CMD_RX_ENABLE   (1 << 3)
#define CMD_TX_ENABLE   (1 << 2)
#define CMD_RESET       (1 << 4)

// Interrupt bits (ISR/IMR)
#define INT_RX_OK       (1 << 0)    // Receive OK
#define INT_RX_ERR      (1 << 1)    // Receive Error
#define INT_TX_OK       (1 << 2)    // Transmit OK
#define INT_TX_ERR      (1 << 3)    // Transmit Error
#define INT_RX_OVERFLOW (1 << 4)    // RX buffer overflow
#define INT_LINK_CHG    (1 << 5)    // Link status change
#define INT_TIMEOUT     (1 << 14)   // Time out

// RX Config bits
#define RX_CFG_AAP      (1 << 0)    // Accept All Packets (promiscuous)
#define RX_CFG_APM      (1 << 1)    // Accept Physical Match (our MAC)
#define RX_CFG_AM       (1 << 2)    // Accept Multicast
#define RX_CFG_AB       (1 << 3)    // Accept Broadcast
#define RX_CFG_WRAP     (1 << 7)    // Wrap mode (ring buffer)

// RX buffer size: 8KB + 16 header + 1500 wrap safety
#define RX_BUF_SIZE     (8192 + 16 + 1500)

// TX buffer size por descriptor
#define TX_BUF_SIZE     RTL8139_BUF_SIZE

// Número de TX descriptors
#define TX_DESC_COUNT   4

// RX packet header (no início de cada pacote no ring)
typedef struct {
    uint16_t status;    // bits: ROK, FAE, CRC, etc.
    uint16_t length;    // tamanho do pacote incluindo CRC (4 bytes)
} __attribute__((packed)) rx_header_t;

#define RX_STATUS_ROK   (1 << 0)   // Receive OK

// ============================================================
// Estado do driver (tudo estático)
// ============================================================
static bool          nic_present = false;
static uint16_t      io_base = 0;           // I/O port base
static uint8_t       mac_addr[ETH_ALEN];    // MAC address
static uint8_t       irq_line = 0;          // IRQ number
static nic_stats_t   stats;

// RX buffer — 3 frames PMM contíguos (12KB, > RX_BUF_SIZE)
// Dentro do identity map, phys == virt
static uint8_t *rx_buffer = NULL;
static uint16_t rx_offset = 0;  // Offset de leitura atual no ring

// TX buffers — 4 descriptors, buffers estáticos
// Alinhados a 4 bytes (uint32_t cast é seguro)
static uint8_t  tx_buffers[TX_DESC_COUNT][TX_BUF_SIZE] __attribute__((aligned(4)));
static uint8_t  tx_current = 0;  // Próximo descriptor a usar

// Callback de recepção
static rtl8139_rx_callback_t rx_callback = NULL;

// ============================================================
// rtl8139_irq_handler — chamado pelo ISR dispatcher
// ============================================================
static void rtl8139_irq_handler(struct isr_frame *frame) {
    (void)frame;

    uint16_t isr_status = inw(io_base + REG_ISR);

    if (isr_status & INT_TX_OK) {
        stats.tx_packets++;
    }

    if (isr_status & INT_TX_ERR) {
        stats.tx_errors++;
    }

    if (isr_status & INT_RX_OK) {
        // Processa todos os pacotes pendentes no ring buffer
        while (!(inb(io_base + REG_CMD) & 0x01)) {  // Bit 0 = buffer empty
            // Lê header do pacote
            rx_header_t *hdr = (rx_header_t *)(rx_buffer + rx_offset);

            if (!(hdr->status & RX_STATUS_ROK)) {
                // Pacote com erro — pula
                stats.rx_errors++;
                break;
            }

            uint16_t pkt_len = hdr->length - 4;  // Remove CRC (4 bytes)

            if (pkt_len > 0 && pkt_len <= RTL8139_BUF_SIZE) {
                stats.rx_packets++;
                stats.rx_bytes += pkt_len;

                // Dados do pacote começam após o header (4 bytes)
                uint8_t *pkt_data = rx_buffer + rx_offset + sizeof(rx_header_t);

                if (rx_callback) {
                    rx_callback(pkt_data, pkt_len);
                }
            }

            // Avança offset: header(4) + length, alinhado a 4 bytes
            rx_offset = (rx_offset + sizeof(rx_header_t) + hdr->length + 3) & ~3;
            rx_offset %= RX_BUF_SIZE;

            // Atualiza CAPR (offset - 16 por quirk do hardware)
            outw(io_base + REG_CAPR, rx_offset - 16);
        }
    }

    if (isr_status & INT_RX_OVERFLOW) {
        stats.rx_errors++;
        // Reset do RX em caso de overflow
        uint8_t cmd = inb(io_base + REG_CMD);
        outb(io_base + REG_CMD, cmd & ~CMD_RX_ENABLE);
        outb(io_base + REG_CMD, cmd | CMD_RX_ENABLE);
        rx_offset = 0;
        outw(io_base + REG_CAPR, 0);
    }

    // Acknowledge todas as interrupções processadas
    outw(io_base + REG_ISR, isr_status);
}

// ============================================================
// rtl8139_init — inicializa a NIC
// ============================================================
bool rtl8139_init(void) {
    // 1. Busca o device no PCI
    pci_device_t dev;
    if (!pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID, &dev)) {
        return false;
    }

    // 2. Extrai I/O base (BAR0 bit 0 = 1 indica I/O space)
    io_base = (uint16_t)(dev.bar0 & ~0x3);  // Remove flags
    irq_line = dev.irq_line;

    // 3. Habilita Bus Mastering (PCI DMA)
    pci_enable_bus_mastering(&dev);

    // 4. Power on — escreve 0x00 no Config1
    outb(io_base + REG_CONFIG1, 0x00);

    // 5. Software reset
    outb(io_base + REG_CMD, CMD_RESET);
    // Espera reset completar (bit RST volta a 0)
    for (int i = 0; i < 100000; i++) {
        if (!(inb(io_base + REG_CMD) & CMD_RESET)) break;
        io_wait();
    }

    // 6. Lê MAC address
    uint32_t mac_low  = inl(io_base + REG_MAC0);
    uint16_t mac_high = inw(io_base + REG_MAC4);
    mac_addr[0] = (uint8_t)(mac_low >>  0);
    mac_addr[1] = (uint8_t)(mac_low >>  8);
    mac_addr[2] = (uint8_t)(mac_low >> 16);
    mac_addr[3] = (uint8_t)(mac_low >> 24);
    mac_addr[4] = (uint8_t)(mac_high >> 0);
    mac_addr[5] = (uint8_t)(mac_high >> 8);

    // 7. Aloca RX buffer — 3 frames PMM (12KB) para 8KB+16+1500 ring
    // Identity map garante phys == virt
    uint32_t frame0 = pmm_alloc_frame();
    uint32_t frame1 = pmm_alloc_frame();
    uint32_t frame2 = pmm_alloc_frame();
    if (!frame0 || !frame1 || !frame2) {
        if (frame0) pmm_free_frame(frame0);
        if (frame1) pmm_free_frame(frame1);
        if (frame2) pmm_free_frame(frame2);
        return false;
    }

    // Verifica se os frames são contíguos
    if (frame1 != frame0 + PMM_FRAME_SIZE || frame2 != frame1 + PMM_FRAME_SIZE) {
        // Não contíguos — tenta de novo (libera e busca numa região limpa)
        pmm_free_frame(frame0);
        pmm_free_frame(frame1);
        pmm_free_frame(frame2);

        // Segunda tentativa: aloca mais frames até achar 3 contíguos
        uint32_t frames[16];
        int allocated = 0;
        bool found = false;

        for (int i = 0; i < 16 && !found; i++) {
            frames[i] = pmm_alloc_frame();
            allocated = i + 1;
            if (!frames[i]) break;

            if (i >= 2) {
                if (frames[i] == frames[i-1] + PMM_FRAME_SIZE &&
                    frames[i-1] == frames[i-2] + PMM_FRAME_SIZE) {
                    frame0 = frames[i-2];
                    found = true;
                }
            }
        }

        // Libera frames que não fazem parte do bloco contíguo
        for (int i = 0; i < allocated; i++) {
            if (frames[i] && (frames[i] < frame0 || frames[i] >= frame0 + 3 * PMM_FRAME_SIZE)) {
                pmm_free_frame(frames[i]);
            }
        }

        if (!found) return false;
    }

    rx_buffer = (uint8_t *)frame0;
    kmemset(rx_buffer, 0, 3 * PMM_FRAME_SIZE);
    rx_offset = 0;

    // 8. Configura RX buffer address
    outl(io_base + REG_RX_BUF, (uint32_t)rx_buffer);

    // 9. Configura Interrupt Mask — ativa RX OK, TX OK, erros
    outw(io_base + REG_IMR, INT_RX_OK | INT_RX_ERR | INT_TX_OK | INT_TX_ERR | INT_RX_OVERFLOW);

    // 10. Configura RX: aceita broadcast + physical match, wrap, buffer 8KB
    // Bits 11:13 = buffer size: 00 = 8K+16
    outl(io_base + REG_RX_CONFIG, RX_CFG_APM | RX_CFG_AB | RX_CFG_AM | RX_CFG_WRAP);

    // 11. Configura TX: defaults (Max DMA burst = 1024, sem IFG)
    outl(io_base + REG_TX_CONFIG, 0x03000000);  // Max DMA burst size

    // 12. Habilita RX e TX
    outb(io_base + REG_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);

    // 13. Registra IRQ handler
    isr_register_handler(IRQ_TO_INT(irq_line), rtl8139_irq_handler);
    pic_unmask_irq(irq_line);

    // 14. Zera stats
    kmemset(&stats, 0, sizeof(stats));

    nic_present = true;
    return true;
}

// ============================================================
// rtl8139_send — envia um frame Ethernet
// ============================================================
bool rtl8139_send(const void *data, uint16_t len) {
    if (!nic_present || !data || len == 0 || len > RTL8139_BUF_SIZE) {
        return false;
    }

    // Copia dados para o TX buffer do descriptor atual
    kmemcpy(tx_buffers[tx_current], data, len);

    // Padding mínimo Ethernet: 60 bytes (sem CRC)
    if (len < 60) {
        kmemset(tx_buffers[tx_current] + len, 0, 60 - len);
        len = 60;
    }

    // Escreve endereço físico do buffer (identity map: virt == phys)
    outl(io_base + REG_TX_ADDR0 + (tx_current * 4), (uint32_t)tx_buffers[tx_current]);

    // Escreve status: tamanho nos bits 0-12, bit 13 = OWN (clear = NIC pode enviar)
    // Threshold bits 16-21 = 0 (começa DMA imediatamente)
    outl(io_base + REG_TX_STATUS0 + (tx_current * 4), (uint32_t)len);

    stats.tx_bytes += len;

    // Avança para próximo descriptor (round-robin)
    tx_current = (tx_current + 1) % TX_DESC_COUNT;

    return true;
}

// ============================================================
// rtl8139_get_mac — copia MAC address
// ============================================================
void rtl8139_get_mac(uint8_t *mac_out) {
    kmemcpy(mac_out, mac_addr, ETH_ALEN);
}

// ============================================================
// rtl8139_is_present — verifica se NIC está ativa
// ============================================================
bool rtl8139_is_present(void) {
    return nic_present;
}

// ============================================================
// rtl8139_get_stats — retorna estatísticas
// ============================================================
nic_stats_t rtl8139_get_stats(void) {
    return stats;
}

// ============================================================
// rtl8139_set_rx_callback — registra callback de recepção
// ============================================================
void rtl8139_set_rx_callback(rtl8139_rx_callback_t cb) {
    rx_callback = cb;
}
