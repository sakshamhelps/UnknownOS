/*
 * Unknown OS - PCI Driver
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * PCI Configuration Space Access (MCFG/ECAM or legacy CF8/CFC)
 * ============================================================ */
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static bool pci_use_ecam = false;
static uint64_t pci_ecam_base = 0;

/* ============================================================
 * PCI Config Read/Write (Legacy CF8/CFC)
 * ============================================================ */
static uint32_t pci_config_read_legacy(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t size) {
    uint32_t address = (1U << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    uint32_t data = inl(PCI_CONFIG_DATA);
    return (data >> ((offset & 3) * 8)) & (0xFFFFFFFF >> (32 - size * 8));
}

static void pci_config_write_legacy(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t size, uint32_t value) {
    uint32_t address = (1U << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    if (size == 4) {
        outl(PCI_CONFIG_DATA, value);
    } else if (size == 2) {
        outw(PCI_CONFIG_DATA + (offset & 2), value);
    } else {
        outb(PCI_CONFIG_DATA + (offset & 3), value);
    }
}

/* ============================================================
 * PCI Config Read/Write (ECAM/MMIO)
 * ============================================================ */
static uint32_t pci_config_read_ecam(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t size) {
    uint64_t addr = pci_ecam_base + (bus << 20) | (slot << 15) | (func << 12) | offset;
    if (size == 4) return *(volatile uint32_t *)addr;
    if (size == 2) return *(volatile uint16_t *)addr;
    return *(volatile uint8_t *)addr;
}

static void pci_config_write_ecam(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t size, uint32_t value) {
    uint64_t addr = pci_ecam_base + (bus << 20) | (slot << 15) | (func << 12) | offset;
    if (size == 4) *(volatile uint32_t *)addr = value;
    else if (size == 2) *(volatile uint16_t *)addr = value;
    else *(volatile uint8_t *)addr = value;
}

/* ============================================================
 * Public PCI Config Access
 * ============================================================ */
uint32_t pci_read_config(struct pci_device *dev, uint8_t offset, uint8_t size) {
    if (pci_use_ecam) {
        return pci_config_read_ecam(dev->bus, dev->slot, dev->func, offset, size);
    } else {
        return pci_config_read_legacy(dev->bus, dev->slot, dev->func, offset, size);
    }
}

void pci_write_config(struct pci_device *dev, uint8_t offset, uint8_t size, uint32_t value) {
    if (pci_use_ecam) {
        pci_config_write_ecam(dev->bus, dev->slot, dev->func, offset, size, value);
    } else {
        pci_config_write_legacy(dev->bus, dev->slot, dev->func, offset, size, value);
    }
}

/* ============================================================
 * PCI Device Enumeration
 * ============================================================ */
static struct pci_device *pci_devices = NULL;
static size_t pci_device_count = 0;
static size_t pci_device_capacity = 0;

static void pci_add_device(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor = pci_config_read_legacy(bus, slot, func, 0x00, 2);
    if (vendor == 0xFFFF) return;  /* No device */

    uint16_t device = pci_config_read_legacy(bus, slot, func, 0x02, 2);
    uint8_t class_code = pci_config_read_legacy(bus, slot, func, 0x0B, 1);
    uint8_t subclass = pci_config_read_legacy(bus, slot, func, 0x0A, 1);
    uint8_t prog_if = pci_config_read_legacy(bus, slot, func, 0x09, 1);
    uint8_t header_type = pci_config_read_legacy(bus, slot, func, 0x0E, 1);

    if (pci_device_count >= pci_device_capacity) {
        pci_device_capacity = pci_device_capacity ? pci_device_capacity * 2 : 32;
        pci_devices = krealloc(pci_devices, pci_device_capacity * sizeof(struct pci_device));
    }

    struct pci_device *dev = &pci_devices[pci_device_count++];
    dev->vendor_id = vendor;
    dev->device_id = device;
    dev->class_code = class_code;
    dev->subclass = subclass;
    dev->prog_if = prog_if;
    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;

    /* Read BARs */
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_config_read_legacy(bus, slot, func, 0x10 + i * 4, 4);
    }

    serial_printf("[PCI] %02x:%02x.%d  %04x:%04x  class %02x/%02x/%02x\n",
                  bus, slot, func, vendor, device, class_code, subclass, prog_if);

    /* Multi-function device? */
    if (func == 0 && (header_type & 0x80)) {
        for (uint8_t f = 1; f < 8; f++) {
            uint16_t v = pci_config_read_legacy(bus, slot, f, 0x00, 2);
            if (v != 0xFFFF) {
                pci_add_device(bus, slot, f);
            }
        }
    }
}

/* ============================================================
 * PCI Initialization
 * ============================================================ */
void pci_init(void) {
    serial_puts("[PCI] Enumerating bus...\n");

    /* Check for MCFG table (ECAM) via ACPI - simplified for now */
    /* For now, use legacy CF8/CFC */

    for (uint8_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_config_read_legacy(bus, slot, 0, 0x00, 2);
            if (vendor != 0xFFFF) {
                pci_add_device(bus, slot, 0);
            }
        }
    }

    serial_printf("[PCI] Found %lu devices\n", pci_device_count);
}

/* ============================================================
 * Device Lookup
 * ============================================================ */
struct pci_device *pci_find_device(uint16_t vendor, uint16_t device) {
    for (size_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor && pci_devices[i].device_id == device) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

struct pci_device *pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (size_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code && pci_devices[i].subclass == subclass) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

/* ============================================================
 * BAR Handling
 * ============================================================ */
uint64_t pci_get_bar(struct pci_device *dev, int bar_idx, bool *is_io, bool *is_64bit) {
    if (bar_idx < 0 || bar_idx >= 6) return 0;
    uint32_t bar = dev->bar[bar_idx];
    if (bar == 0) return 0;

    *is_io = (bar & 0x1);
    *is_64bit = (!*is_io && (bar & 0x6) == 0x4);

    uint64_t addr = bar & ~0xF;
    if (*is_64bit && bar_idx < 5) {
        addr |= ((uint64_t)dev->bar[bar_idx + 1]) << 32;
    }
    return addr;
}