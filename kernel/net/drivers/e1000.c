#include "drivers/e1000.h"
#include "net/netdevice.h"
#include "drivers/pci.h"
#include "console.h"
#include "heap.h"
#include "string.h"
#include "pmm.h"
#include "vmm.h"
#include "drivers/irq.h"
#include "net/skbuff.h"
#include "net/ethernet.h"

#define E1000_RX_BUFFER_SIZE 2048
#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 8

struct e1000_adapter {
    uint64_t mmio_base;
    uint64_t mmio_len;
    
    struct e1000_rx_desc *rx_descs;  
    struct e1000_tx_desc *tx_descs;  
    
    uint64_t rx_descs_phys;
    uint64_t tx_descs_phys;
    
    void **rx_buffers;  

    struct sk_buff *tx_skbs[E1000_NUM_TX_DESC];  
    
    uint16_t rx_cur;
    uint16_t tx_cur;
    uint16_t tx_clean;  
    
    struct net_device *netdev;
    struct pci_device *pci_dev;
};

static inline void e1000_write_command(struct e1000_adapter *adapter, uint16_t addr, uint32_t val) {
    *((volatile uint32_t*)(adapter->mmio_base + addr)) = val;
}

static inline uint32_t e1000_read_command(struct e1000_adapter *adapter, uint16_t addr) {
    return *((volatile uint32_t*)(adapter->mmio_base + addr));
}

static void e1000_eeprom_detect(struct e1000_adapter *adapter) {
    uint32_t val = 0;
    e1000_write_command(adapter, E1000_EERD, 0x1); 

    int i = 0;
    while(i < 1000 && !((val = e1000_read_command(adapter, E1000_EERD)) & (1<<4))) {
        i++;
    }
}

static uint16_t e1000_eeprom_read(struct e1000_adapter *adapter, uint8_t addr) {
    uint32_t tmp = 0;
    if (1) {  
        e1000_write_command(adapter, E1000_EERD, 1 | ((uint32_t)addr << 8));
        while(!((tmp = e1000_read_command(adapter, E1000_EERD)) & (1 << 4)));
    }
    return (uint16_t)((tmp >> 16) & 0xFFFF);
}

static void e1000_read_mac(struct e1000_adapter *adapter) {
    uint16_t offset = 0;
    uint16_t val = e1000_eeprom_read(adapter, offset);
    adapter->netdev->dev_addr[0] = val & 0xFF;
    adapter->netdev->dev_addr[1] = val >> 8;
    
    offset = 1;
    val = e1000_eeprom_read(adapter, offset);
    adapter->netdev->dev_addr[2] = val & 0xFF;
    adapter->netdev->dev_addr[3] = val >> 8;
    
    offset = 2;
    val = e1000_eeprom_read(adapter, offset);
    adapter->netdev->dev_addr[4] = val & 0xFF;
    adapter->netdev->dev_addr[5] = val >> 8;

    memset(adapter->netdev->broadcast, 0xFF, 6);
}

static int e1000_open(struct net_device *dev) {
    (void)dev;
    kprint_str("E1000: Device Opened\n");
    return 0;
}

static int e1000_stop(struct net_device *dev) {
    (void)dev;
    kprint_str("E1000: Device Stopped\n");
    return 0;
}

static void e1000_clean_tx(struct e1000_adapter *adapter) {
    uint16_t i = adapter->tx_clean;
    while (i != adapter->tx_cur) {
        struct e1000_tx_desc *desc = &adapter->tx_descs[i];

        if (desc->status & 1) {  
             
            if (adapter->tx_skbs[i]) {
                kfree_skb(adapter->tx_skbs[i]);
                adapter->tx_skbs[i] = NULL;
            }
            
            desc->status = 0;  
            i = (i + 1) % E1000_NUM_TX_DESC;
        } else {
             
            break; 
        }
    }
    adapter->tx_clean = i;
}

static int e1000_start_xmit(struct sk_buff *skb, struct net_device *dev) {
    struct e1000_adapter *adapter = (struct e1000_adapter *)dev->priv;

    e1000_clean_tx(adapter);

    uint16_t cur = adapter->tx_cur;
    struct e1000_tx_desc *desc = &adapter->tx_descs[cur];

    uint16_t next = (cur + 1) % E1000_NUM_TX_DESC;
    if (next == adapter->tx_clean) {
        kprint_str("E1000: TX Ring Full\n");
         
        return -1; 
    }

    uint64_t phys_addr = vmm_get_phys((uint64_t)skb->data);
    
    desc->addr = phys_addr;
    desc->length = skb->len;
    desc->cmd = E1000_CMD_EOP | E1000_CMD_IFCS | E1000_CMD_RS; 
    desc->status = 0;

    adapter->tx_skbs[cur] = skb;

    adapter->tx_cur = next;
    
    e1000_write_command(adapter, E1000_TDT, adapter->tx_cur);
    
    return 0;
}

static void e1000_handle_receive(struct e1000_adapter *adapter) {
    while ((adapter->rx_descs[adapter->rx_cur].status & 1)) {
        uint8_t *buf = (uint8_t *)adapter->rx_buffers[adapter->rx_cur];
        uint16_t len = adapter->rx_descs[adapter->rx_cur].length;

        struct sk_buff *skb = alloc_skb(len + 2);
        if (!skb) {
            kprint_str("E1000: Dropping packet, no memory\n");
        } else {
            skb->dev = adapter->netdev;
            skb_reserve(skb, 2);  
            
            memcpy(skb->data, buf, len);
            skb_put(skb, len);
            
            skb->protocol = eth_type_trans(skb, adapter->netdev);
            
             
            netif_rx(skb);
        }

        adapter->rx_descs[adapter->rx_cur].status = 0;

        uint16_t old_cur = adapter->rx_cur;
        adapter->rx_cur = (adapter->rx_cur + 1) % E1000_NUM_RX_DESC;
        
        e1000_write_command(adapter, E1000_RDT, old_cur);
    }
}

static irqreturn_t e1000_irq_handler(int irq, void *dev_id) {
    (void)irq;
    struct net_device *dev = (struct net_device *)dev_id;
    struct e1000_adapter *adapter = (struct e1000_adapter *)dev->priv;
    
    uint32_t status = e1000_read_command(adapter, E1000_ICR);
    
    if (status & E1000_ICR_LSC) {
        adapter->netdev->flags |= IFF_RUNNING;
        kprint_str("E1000: Link Status Change\n");
    }
    
    if (status & E1000_ICR_RXT0) {
        e1000_handle_receive(adapter);
    }
    
    return IRQ_HANDLED;
}

static void e1000_setup_rx(struct e1000_adapter *adapter) {
     
    adapter->rx_descs = (struct e1000_rx_desc *)kmalloc(sizeof(struct e1000_rx_desc) * E1000_NUM_RX_DESC);
    adapter->rx_descs_phys = vmm_get_phys((uint64_t)adapter->rx_descs);
    memset(adapter->rx_descs, 0, sizeof(struct e1000_rx_desc) * E1000_NUM_RX_DESC);
    
    adapter->rx_buffers = (void **)kmalloc(sizeof(void*) * E1000_NUM_RX_DESC);
    
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        adapter->rx_buffers[i] = kmalloc(E1000_RX_BUFFER_SIZE);
        adapter->rx_descs[i].addr = vmm_get_phys((uint64_t)adapter->rx_buffers[i]);
        adapter->rx_descs[i].status = 0;
    }
    
    e1000_write_command(adapter, E1000_RDBAL, adapter->rx_descs_phys & 0xFFFFFFFF);
    e1000_write_command(adapter, E1000_RDBAH, adapter->rx_descs_phys >> 32);
    
    e1000_write_command(adapter, E1000_RDLEN, sizeof(struct e1000_rx_desc) * E1000_NUM_RX_DESC);
    
    e1000_write_command(adapter, E1000_RDH, 0);
    e1000_write_command(adapter, E1000_RDT, E1000_NUM_RX_DESC - 1);
    adapter->rx_cur = 0;
    
    e1000_write_command(adapter, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_SBP | E1000_RCTL_UPE | E1000_RCTL_MPE | E1000_RCTL_LPE | E1000_RCTL_BAM | E1000_RCTL_BAM);
}

static void e1000_setup_tx(struct e1000_adapter *adapter) {
    adapter->tx_descs = (struct e1000_tx_desc *)kmalloc(sizeof(struct e1000_tx_desc) * E1000_NUM_TX_DESC);
    adapter->tx_descs_phys = vmm_get_phys((uint64_t)adapter->tx_descs);
    memset(adapter->tx_descs, 0, sizeof(struct e1000_tx_desc) * E1000_NUM_TX_DESC);
    
    e1000_write_command(adapter, E1000_TDBAL, adapter->tx_descs_phys & 0xFFFFFFFF);
    e1000_write_command(adapter, E1000_TDBAH, adapter->tx_descs_phys >> 32);
    
    e1000_write_command(adapter, E1000_TDLEN, sizeof(struct e1000_tx_desc) * E1000_NUM_TX_DESC);
    
    e1000_write_command(adapter, E1000_TDH, 0);
    e1000_write_command(adapter, E1000_TDT, 0);
    adapter->tx_cur = 0;
    adapter->tx_clean = 0;
    memset(adapter->tx_skbs, 0, sizeof(adapter->tx_skbs));
    
    e1000_write_command(adapter, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP);
}

static struct net_device_ops e1000_netdev_ops = {
    .open = e1000_open,
    .stop = e1000_stop,
    .start_xmit = e1000_start_xmit,
};

static int e1000_probe(struct pci_device *pdev) {
    kprint_str("E1000: Found device at ");
    kprint_hex(pdev->bus); kprint_str(":"); kprint_hex(pdev->slot); kprint_str(":"); kprint_hex(pdev->func);
    kprint_newline();
    
     
    uint16_t cmd = pci_read_word(pdev->bus, pdev->slot, pdev->func, PCI_COMMAND);
    cmd |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;
    pci_write_word(pdev->bus, pdev->slot, pdev->func, PCI_COMMAND, cmd);
    
    struct net_device *netdev = alloc_netdev(sizeof(struct e1000_adapter), "eth0", NULL);
    if (!netdev) return -1;
    
    struct e1000_adapter *adapter = (struct e1000_adapter *)netdev->priv;
    adapter->netdev = netdev;
    adapter->pci_dev = pdev;
    
    uint64_t mmio_phys = (uint64_t)pdev->bar[0] & ~0xF;
    uint64_t mmio_len = 128 * 1024;  
    
    void *mmio_virt = ioremap(mmio_phys, mmio_len);
    if (!mmio_virt) {
        kprint_str("E1000: Failed to map MMIO\n");
        return -1;
    }
    
    adapter->mmio_base = (uint64_t)mmio_virt;
    adapter->mmio_len = mmio_len;
    kprint_str("E1000: MMIO Base: "); kprint_hex(adapter->mmio_base); kprint_newline();
    
    e1000_eeprom_detect(adapter);
    e1000_read_mac(adapter);
    
    kprint_str("E1000: MAC: ");
    for(int i=0; i<6; i++) {
        kprint_hex_byte(netdev->dev_addr[i]);
        if (i<5) kprint_str(":");
    }
    kprint_newline();
    
    netdev->netdev_ops = &e1000_netdev_ops;
    
     
    e1000_write_command(adapter, E1000_CTRL, E1000_CTRL_SLU);  
    
     
    e1000_write_command(adapter, E1000_IMC, 0xFFFFFFFF);  
    e1000_write_command(adapter, E1000_ICR, 0xFFFFFFFF);  
    
    request_irq(pdev->interrupt_line, e1000_irq_handler, 0, "e1000", netdev);
    
    e1000_write_command(adapter, E1000_IMS, E1000_ICR_LSC | E1000_ICR_RXT0);  
    
    e1000_setup_rx(adapter);
    e1000_setup_tx(adapter);
    
    register_netdev(netdev);
    
    return 0;
}

static struct pci_driver e1000_driver = {
    .driver = {
        .name = "e1000",
    },
    .vendor_id = INTEL_VEND,
    .device_id = E1000_DEV,  
};

static int e1000_probe_wrapper(struct device *dev) {
     
     
    struct pci_device *pdev = to_pci_device(dev);
    return e1000_probe(pdev);
}

void e1000_init_driver() {
    e1000_driver.driver.probe = e1000_probe_wrapper;
    pci_register_driver(&e1000_driver);
}
