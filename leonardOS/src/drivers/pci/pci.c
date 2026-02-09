// LeonardOS - PCI Bus Driver
// Implementação do acesso ao barramento PCI via Configuration Space
//
// O PCI usa duas portas I/O de 32 bits:
//   0xCF8 (CONFIG_ADDRESS) — endereço do registrador a acessar
//   0xCFC (CONFIG_DATA)    — dados lidos/escritos
//
// O endereço é formado por: bus(8) | slot(5) | func(3) | offset(8) | enable(1)

#include "pci.h"
#include "../../common/io.h"

// ============================================================
// pci_config_addr — monta o endereço PCI
// ============================================================
static uint32_t pci_config_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)(
        ((uint32_t)1     << 31) |   // Enable bit
        ((uint32_t)bus   << 16) |
        ((uint32_t)(slot & 0x1F) << 11) |
        ((uint32_t)(func & 0x07) << 8)  |
        ((uint32_t)(offset & 0xFC))     // Alinhado a 4 bytes
    );
}

// ============================================================
// pci_config_read32 — lê 32 bits do config space
// ============================================================
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDR, pci_config_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

// ============================================================
// pci_config_write32 — escreve 32 bits no config space
// ============================================================
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDR, pci_config_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, value);
}

// ============================================================
// pci_config_read16 — lê 16 bits do config space
// ============================================================
uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_config_read32(bus, slot, func, offset & 0xFC);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

// ============================================================
// pci_config_write16 — escreve 16 bits no config space
// ============================================================
void pci_config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t addr_aligned = offset & 0xFC;
    uint32_t old = pci_config_read32(bus, slot, func, addr_aligned);
    int shift = (offset & 2) * 8;
    uint32_t mask = ~(0xFFFF << shift);
    uint32_t new_val = (old & mask) | ((uint32_t)value << shift);
    pci_config_write32(bus, slot, func, addr_aligned, new_val);
}

// ============================================================
// pci_config_read8 — lê 8 bits do config space
// ============================================================
uint8_t pci_config_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_config_read32(bus, slot, func, offset & 0xFC);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

// ============================================================
// pci_find_device — scan do barramento para achar vendor:device
// ============================================================
bool pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t *dev) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vid = pci_config_read16((uint8_t)bus, slot, func, PCI_REG_VENDOR_ID);
                if (vid == 0xFFFF) {
                    // Slot vazio — se func 0 não existe, skip funções
                    if (func == 0) break;
                    continue;
                }

                uint16_t did = pci_config_read16((uint8_t)bus, slot, func, PCI_REG_DEVICE_ID);
                if (vid == vendor_id && did == device_id) {
                    dev->bus       = (uint8_t)bus;
                    dev->slot      = slot;
                    dev->func      = func;
                    dev->vendor_id = vid;
                    dev->device_id = did;
                    dev->class_code = pci_config_read8((uint8_t)bus, slot, func, PCI_REG_CLASS);
                    dev->subclass   = pci_config_read8((uint8_t)bus, slot, func, PCI_REG_SUBCLASS);
                    dev->irq_line   = pci_config_read8((uint8_t)bus, slot, func, PCI_REG_IRQ_LINE);
                    dev->bar0       = pci_config_read32((uint8_t)bus, slot, func, PCI_REG_BAR0);
                    dev->present    = true;
                    return true;
                }

                // Se func 0 não é multifunction, skip funções 1-7
                if (func == 0) {
                    uint8_t header = pci_config_read8((uint8_t)bus, slot, func, PCI_REG_HEADER_TYPE);
                    if (!(header & 0x80)) break;  // Bit 7 = multifunction
                }
            }
        }
    }

    dev->present = false;
    return false;
}

// ============================================================
// pci_enable_bus_mastering — habilita DMA para um device
// ============================================================
void pci_enable_bus_mastering(pci_device_t *dev) {
    uint16_t cmd = pci_config_read16(dev->bus, dev->slot, dev->func, PCI_REG_COMMAND);
    cmd |= PCI_CMD_BUS_MASTER | PCI_CMD_IO_SPACE;
    pci_config_write16(dev->bus, dev->slot, dev->func, PCI_REG_COMMAND, cmd);
}
