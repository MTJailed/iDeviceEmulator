/*
 * QEMU AMD PC-Net II (Am79C970A) PCI emulation
 *
 * Copyright (c) 2004 Antony T Curtis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* This software was written to be compatible with the specification:
 * AMD Am79C970A PCnet-PCI II Ethernet Controller Data-Sheet
 * AMD Publication# 19436  Rev:E  Amendment/0  Issue Date: June 2000
 */

#include "pci.h"
#include "net.h"
#include "loader.h"
#include "qemu-timer.h"

#include "pcnet.h"

//#define PCNET_DEBUG
//#define PCNET_DEBUG_IO
//#define PCNET_DEBUG_BCR
//#define PCNET_DEBUG_CSR
//#define PCNET_DEBUG_RMD
//#define PCNET_DEBUG_TMD
//#define PCNET_DEBUG_MATCH


typedef struct {
    PCIDevice pci_dev;
    PCNetState state;
} PCIPCNetState;

static void pcnet_aprom_writeb(void *opaque, uint32_t addr, uint32_t val)
{
    PCNetState *s = opaque;
#ifdef PCNET_DEBUG
    printf("pcnet_aprom_writeb addr=0x%08x val=0x%02x\n", addr, val);
#endif
    /* Check APROMWE bit to enable write access */
    if (pcnet_bcr_readw(s,2) & 0x100)
        s->prom[addr & 15] = val;
}

static uint32_t pcnet_aprom_readb(void *opaque, uint32_t addr)
{
    PCNetState *s = opaque;
    uint32_t val = s->prom[addr & 15];
#ifdef PCNET_DEBUG
    printf("pcnet_aprom_readb addr=0x%08x val=0x%02x\n", addr, val);
#endif
    return val;
}

static void pcnet_ioport_map(PCIDevice *pci_dev, int region_num,
                             pcibus_t addr, pcibus_t size, int type)
{
    PCNetState *d = &DO_UPCAST(PCIPCNetState, pci_dev, pci_dev)->state;

#ifdef PCNET_DEBUG_IO
    printf("pcnet_ioport_map addr=0x%04"FMT_PCIBUS" size=0x%04"FMT_PCIBUS"\n",
           addr, size);
#endif

    register_ioport_write(addr, 16, 1, pcnet_aprom_writeb, d);
    register_ioport_read(addr, 16, 1, pcnet_aprom_readb, d);

    register_ioport_write(addr + 0x10, 0x10, 2, pcnet_ioport_writew, d);
    register_ioport_read(addr + 0x10, 0x10, 2, pcnet_ioport_readw, d);
    register_ioport_write(addr + 0x10, 0x10, 4, pcnet_ioport_writel, d);
    register_ioport_read(addr + 0x10, 0x10, 4, pcnet_ioport_readl, d);
}

static void pcnet_mmio_writeb(void *opaque, target_phys_addr_t addr, uint32_t val)
{
    PCNetState *d = opaque;
#ifdef PCNET_DEBUG_IO
    printf("pcnet_mmio_writeb addr=0x" TARGET_FMT_plx" val=0x%02x\n", addr,
           val);
#endif
    if (!(addr & 0x10))
        pcnet_aprom_writeb(d, addr & 0x0f, val);
}

static uint32_t pcnet_mmio_readb(void *opaque, target_phys_addr_t addr)
{
    PCNetState *d = opaque;
    uint32_t val = -1;
    if (!(addr & 0x10))
        val = pcnet_aprom_readb(d, addr & 0x0f);
#ifdef PCNET_DEBUG_IO
    printf("pcnet_mmio_readb addr=0x" TARGET_FMT_plx " val=0x%02x\n", addr,
           val & 0xff);
#endif
    return val;
}

static void pcnet_mmio_writew(void *opaque, target_phys_addr_t addr, uint32_t val)
{
    PCNetState *d = opaque;
#ifdef PCNET_DEBUG_IO
    printf("pcnet_mmio_writew addr=0x" TARGET_FMT_plx " val=0x%04x\n", addr,
           val);
#endif
    if (addr & 0x10)
        pcnet_ioport_writew(d, addr & 0x0f, val);
    else {
        addr &= 0x0f;
        pcnet_aprom_writeb(d, addr, val & 0xff);
        pcnet_aprom_writeb(d, addr+1, (val & 0xff00) >> 8);
    }
}

static uint32_t pcnet_mmio_readw(void *opaque, target_phys_addr_t addr)
{
    PCNetState *d = opaque;
    uint32_t val = -1;
    if (addr & 0x10)
        val = pcnet_ioport_readw(d, addr & 0x0f);
    else {
        addr &= 0x0f;
        val = pcnet_aprom_readb(d, addr+1);
        val <<= 8;
        val |= pcnet_aprom_readb(d, addr);
    }
#ifdef PCNET_DEBUG_IO
    printf("pcnet_mmio_readw addr=0x" TARGET_FMT_plx" val = 0x%04x\n", addr,
           val & 0xffff);
#endif
    return val;
}

static void pcnet_mmio_writel(void *opaque, target_phys_addr_t addr, uint32_t val)
{
    PCNetState *d = opaque;
#ifdef PCNET_DEBUG_IO
    printf("pcnet_mmio_writel addr=0x" TARGET_FMT_plx" val=0x%08x\n", addr,
           val);
#endif
    if (addr & 0x10)
        pcnet_ioport_writel(d, addr & 0x0f, val);
    else {
        addr &= 0x0f;
        pcnet_aprom_writeb(d, addr, val & 0xff);
        pcnet_aprom_writeb(d, addr+1, (val & 0xff00) >> 8);
        pcnet_aprom_writeb(d, addr+2, (val & 0xff0000) >> 16);
        pcnet_aprom_writeb(d, addr+3, (val & 0xff000000) >> 24);
    }
}

static uint32_t pcnet_mmio_readl(void *opaque, target_phys_addr_t addr)
{
    PCNetState *d = opaque;
    uint32_t val;
    if (addr & 0x10)
        val = pcnet_ioport_readl(d, addr & 0x0f);
    else {
        addr &= 0x0f;
        val = pcnet_aprom_readb(d, addr+3);
        val <<= 8;
        val |= pcnet_aprom_readb(d, addr+2);
        val <<= 8;
        val |= pcnet_aprom_readb(d, addr+1);
        val <<= 8;
        val |= pcnet_aprom_readb(d, addr);
    }
#ifdef PCNET_DEBUG_IO
    printf("pcnet_mmio_readl addr=0x" TARGET_FMT_plx " val=0x%08x\n", addr,
           val);
#endif
    return val;
}

static const VMStateDescription vmstate_pci_pcnet = {
    .name = "pcnet",
    .version_id = 3,
    .minimum_version_id = 2,
    .minimum_version_id_old = 2,
    .fields      = (VMStateField []) {
        VMSTATE_PCI_DEVICE(pci_dev, PCIPCNetState),
        VMSTATE_STRUCT(state, PCIPCNetState, 0, vmstate_pcnet, PCNetState),
        VMSTATE_END_OF_LIST()
    }
};

/* PCI interface */

static CPUWriteMemoryFunc * const pcnet_mmio_write[] = {
    &pcnet_mmio_writeb,
    &pcnet_mmio_writew,
    &pcnet_mmio_writel
};

static CPUReadMemoryFunc * const pcnet_mmio_read[] = {
    &pcnet_mmio_readb,
    &pcnet_mmio_readw,
    &pcnet_mmio_readl
};

static void pcnet_mmio_map(PCIDevice *pci_dev, int region_num,
                            pcibus_t addr, pcibus_t size, int type)
{
    PCIPCNetState *d = DO_UPCAST(PCIPCNetState, pci_dev, pci_dev);

#ifdef PCNET_DEBUG_IO
    printf("pcnet_mmio_map addr=0x%08"FMT_PCIBUS" 0x%08"FMT_PCIBUS"\n",
           addr, size);
#endif

    cpu_register_physical_memory(addr, PCNET_PNPMMIO_SIZE, d->state.mmio_index);
}

static void pci_physical_memory_write(void *dma_opaque, target_phys_addr_t addr,
                                      uint8_t *buf, int len, int do_bswap)
{
    cpu_physical_memory_write(addr, buf, len);
}

static void pci_physical_memory_read(void *dma_opaque, target_phys_addr_t addr,
                                     uint8_t *buf, int len, int do_bswap)
{
    cpu_physical_memory_read(addr, buf, len);
}

static void pci_pcnet_cleanup(VLANClientState *nc)
{
    PCNetState *d = DO_UPCAST(NICState, nc, nc)->opaque;

    pcnet_common_cleanup(d);
}

static int pci_pcnet_uninit(PCIDevice *dev)
{
    PCIPCNetState *d = DO_UPCAST(PCIPCNetState, pci_dev, dev);

    cpu_unregister_io_memory(d->state.mmio_index);
    qemu_del_timer(d->state.poll_timer);
    qemu_free_timer(d->state.poll_timer);
    qemu_del_vlan_client(&d->state.nic->nc);
    return 0;
}

static NetClientInfo net_pci_pcnet_info = {
    .type = NET_CLIENT_TYPE_NIC,
    .size = sizeof(NICState),
    .can_receive = pcnet_can_receive,
    .receive = pcnet_receive,
    .cleanup = pci_pcnet_cleanup,
};

static int pci_pcnet_init(PCIDevice *pci_dev)
{
    PCIPCNetState *d = DO_UPCAST(PCIPCNetState, pci_dev, pci_dev);
    PCNetState *s = &d->state;
    uint8_t *pci_conf;

#if 0
    printf("sizeof(RMD)=%d, sizeof(TMD)=%d\n",
        sizeof(struct pcnet_RMD), sizeof(struct pcnet_TMD));
#endif

    pci_conf = pci_dev->config;

    pci_config_set_vendor_id(pci_conf, PCI_VENDOR_ID_AMD);
    pci_config_set_device_id(pci_conf, PCI_DEVICE_ID_AMD_LANCE);
    pci_set_word(pci_conf + PCI_STATUS,
                 PCI_STATUS_FAST_BACK | PCI_STATUS_DEVSEL_MEDIUM);
    pci_conf[PCI_REVISION_ID] = 0x10;
    pci_config_set_class(pci_conf, PCI_CLASS_NETWORK_ETHERNET);

    pci_set_word(pci_conf + PCI_SUBSYSTEM_VENDOR_ID, 0x0);
    pci_set_word(pci_conf + PCI_SUBSYSTEM_ID, 0x0);

    pci_conf[PCI_INTERRUPT_PIN] = 1; // interrupt pin 0
    pci_conf[PCI_MIN_GNT] = 0x06;
    pci_conf[PCI_MAX_LAT] = 0xff;

    /* Handler for memory-mapped I/O */
    s->mmio_index =
      cpu_register_io_memory(pcnet_mmio_read, pcnet_mmio_write, &d->state,
                             DEVICE_NATIVE_ENDIAN);

    pci_register_bar(pci_dev, 0, PCNET_IOPORT_SIZE,
                           PCI_BASE_ADDRESS_SPACE_IO, pcnet_ioport_map);

    pci_register_bar(pci_dev, 1, PCNET_PNPMMIO_SIZE,
                           PCI_BASE_ADDRESS_SPACE_MEMORY, pcnet_mmio_map);

    s->irq = pci_dev->irq[0];
    s->phys_mem_read = pci_physical_memory_read;
    s->phys_mem_write = pci_physical_memory_write;

    if (!pci_dev->qdev.hotplugged) {
        static int loaded = 0;
        if (!loaded) {
            rom_add_option("pxe-pcnet.bin", -1);
            loaded = 1;
        }
    }

    return pcnet_common_init(&pci_dev->qdev, s, &net_pci_pcnet_info);
}

static void pci_reset(DeviceState *dev)
{
    PCIPCNetState *d = DO_UPCAST(PCIPCNetState, pci_dev.qdev, dev);

    pcnet_h_reset(&d->state);
}

static PCIDeviceInfo pcnet_info = {
    .qdev.name  = "pcnet",
    .qdev.size  = sizeof(PCIPCNetState),
    .qdev.reset = pci_reset,
    .qdev.vmsd  = &vmstate_pci_pcnet,
    .init       = pci_pcnet_init,
    .exit       = pci_pcnet_uninit,
    .qdev.props = (Property[]) {
        DEFINE_NIC_PROPERTIES(PCIPCNetState, state.conf),
        DEFINE_PROP_END_OF_LIST(),
    }
};

static void pci_pcnet_register_devices(void)
{
    pci_qdev_register(&pcnet_info);
}

device_init(pci_pcnet_register_devices)
