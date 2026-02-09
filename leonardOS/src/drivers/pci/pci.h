// LeonardOS - PCI Bus Driver
// Scan e acesso ao barramento PCI via Configuration Space (portas 0xCF8/0xCFC)

#ifndef __PCI_H__
#define __PCI_H__

#include "../../common/types.h"

// Portas do PCI Configuration Space
#define PCI_CONFIG_ADDR  0x0CF8
#define PCI_CONFIG_DATA  0x0CFC

// Offsets de registradores PCI padrão
#define PCI_REG_VENDOR_ID    0x00  // 16-bit
#define PCI_REG_DEVICE_ID    0x02  // 16-bit
#define PCI_REG_COMMAND      0x04  // 16-bit
#define PCI_REG_STATUS       0x06  // 16-bit
#define PCI_REG_CLASS        0x0B  // 8-bit (class code)
#define PCI_REG_SUBCLASS     0x0A  // 8-bit
#define PCI_REG_HEADER_TYPE  0x0E  // 8-bit
#define PCI_REG_BAR0         0x10  // 32-bit (Base Address Register 0)
#define PCI_REG_BAR1         0x14  // 32-bit
#define PCI_REG_BAR2         0x18  // 32-bit
#define PCI_REG_BAR3         0x1C  // 32-bit
#define PCI_REG_BAR4         0x20  // 32-bit
#define PCI_REG_BAR5         0x24  // 32-bit
#define PCI_REG_IRQ_LINE     0x3C  // 8-bit (interrupt line)
#define PCI_REG_IRQ_PIN      0x3D  // 8-bit (interrupt pin)

// Bits do PCI Command Register
#define PCI_CMD_IO_SPACE     (1 << 0)   // Habilita I/O space
#define PCI_CMD_MEMORY       (1 << 1)   // Habilita Memory space
#define PCI_CMD_BUS_MASTER   (1 << 2)   // Habilita bus mastering (DMA)

// Informação de um dispositivo PCI encontrado
typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  irq_line;
    uint32_t bar0;       // Base Address Register 0
    bool     present;
} pci_device_t;

// ============================================================
// API pública
// ============================================================

// Lê 32 bits do PCI Configuration Space
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

// Escreve 32 bits no PCI Configuration Space
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

// Lê 16 bits do PCI Configuration Space
uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

// Escreve 16 bits no PCI Configuration Space
void pci_config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);

// Lê 8 bits do PCI Configuration Space
uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

// Busca dispositivo por vendor_id e device_id
// Retorna true se encontrado, preenche *dev
bool pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t *dev);

// Habilita Bus Mastering (DMA) para um dispositivo
void pci_enable_bus_mastering(pci_device_t *dev);

#endif
