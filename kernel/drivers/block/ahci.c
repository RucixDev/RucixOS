#include "drivers/ahci.h"
#include "drivers/pci.h"
#include "console.h"
#include "heap.h"
#include "string.h"
#include "vmm.h"
#include "pmm.h"
#include "drivers/irq.h"

#define AHCI_PORT_COUNT 32

static struct ahci_device *ahci_devs[4];  
static int ahci_dev_count = 0;

static int check_type(hba_port_t *port) {
    uint32_t ssts = port->ssts;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;
    
    if (det != 3) return 0;  
    if (ipm != 1) return 0;  
    
    switch (port->sig) {
        case 0xEB140101: return 2;  
        case 0xC33C0101: return 3;  
        case 0x96690101: return 4;  
        default: return 1;  
    }
}

static void start_cmd(hba_port_t *port) {
    while (port->cmd & AHCI_PxCMD_CR);
    port->cmd |= AHCI_PxCMD_FRE;
    port->cmd |= AHCI_PxCMD_ST;
}

static void stop_cmd(hba_port_t *port) {
    port->cmd &= ~AHCI_PxCMD_ST;
    port->cmd &= ~AHCI_PxCMD_FRE;
    while(1) {
        if (port->cmd & AHCI_PxCMD_FR) continue;
        if (port->cmd & AHCI_PxCMD_CR) continue;
        break;
    }
}

static int ahci_read(hba_port_t *port, uint32_t startl, uint32_t starth, uint32_t count, uint16_t *buf) {
    port->is = (uint32_t) -1;  
    int slot = 0;
    
     
    uint32_t slots = (port->sact | port->ci);
    for (int i=0; i<32; i++) {
        if ((slots & (1 << i)) == 0) {
            slot = i;
            break;
        }
    }

    struct ahci_device *adev = 0;
    int port_idx = -1;
    for (int i=0; i<ahci_dev_count; i++) {
        uint64_t diff = (uint64_t)port - (uint64_t)ahci_devs[i]->abar->ports;
        if (diff < sizeof(hba_port_t) * 32 && (diff % sizeof(hba_port_t)) == 0) {
            adev = ahci_devs[i];
            port_idx = diff / sizeof(hba_port_t);
            break;
        }
    }
    
    if (!adev || port_idx == -1) return -1;
    
    struct ahci_cmd_header *cmdheader = (struct ahci_cmd_header*)(adev->port_clb[port_idx]);
    cmdheader += slot;
    
    cmdheader->cfl = sizeof(struct fis_reg_h2d)/4;
    cmdheader->w = 0;  
    cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1;  
    
    struct ahci_tbl *cmdtbl = (struct ahci_tbl*)(adev->port_ctba[port_idx][slot]);
    memset(cmdtbl, 0, sizeof(struct ahci_tbl) + (cmdheader->prdtl-1)*sizeof(struct ahci_prdt_entry));

    uint64_t phys_buf = vmm_get_phys((uint64_t)buf);
    
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buf & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)((phys_buf >> 32) & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1;  
    cmdtbl->prdt_entry[0].i = 1;

    struct fis_reg_h2d *cmdfis = (struct fis_reg_h2d*)(&cmdtbl->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;  
    cmdfis->command = ATA_CMD_READ_DMA_EX;
    
    cmdfis->lba0 = (uint8_t)startl;
    cmdfis->lba1 = (uint8_t)(startl >> 8);
    cmdfis->lba2 = (uint8_t)(startl >> 16);
    cmdfis->device = 1<<6;  
    
    cmdfis->lba3 = (uint8_t)(startl >> 24);
    cmdfis->lba4 = (uint8_t)starth;
    cmdfis->lba5 = (uint8_t)(starth >> 8);
    
    cmdfis->countl = count & 0xFF;
    cmdfis->counth = (count >> 8) & 0xFF;

    while (port->tfd & (ATA_SR_BSY | ATA_SR_DRQ));
    
    port->ci |= (1 << slot);

    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) {
            return -1;  
        }
    }
    
    if (port->is & HBA_PxIS_TFES) {
        return -1;
    }
    
    return 0;
}

static int ahci_write(hba_port_t *port, uint32_t startl, uint32_t starth, uint32_t count, uint16_t *buf) {
    port->is = (uint32_t) -1;
    int slot = 0;
    
    uint32_t slots = (port->sact | port->ci);
    for (int i=0; i<32; i++) {
        if ((slots & (1 << i)) == 0) {
            slot = i;
            break;
        }
    }
    
    struct ahci_device *adev = 0;
    int port_idx = -1;
    for (int i=0; i<ahci_dev_count; i++) {
        uint64_t diff = (uint64_t)port - (uint64_t)ahci_devs[i]->abar->ports;
        if (diff < sizeof(hba_port_t) * 32 && (diff % sizeof(hba_port_t)) == 0) {
            adev = ahci_devs[i];
            port_idx = diff / sizeof(hba_port_t);
            break;
        }
    }
    
    if (!adev || port_idx == -1) return -1;
    
    struct ahci_cmd_header *cmdheader = (struct ahci_cmd_header*)(adev->port_clb[port_idx]);
    cmdheader += slot;
    
    cmdheader->cfl = sizeof(struct fis_reg_h2d)/4;
    cmdheader->w = 1;  
    cmdheader->prdtl = 1;
    
    struct ahci_tbl *cmdtbl = (struct ahci_tbl*)(adev->port_ctba[port_idx][slot]);
    memset(cmdtbl, 0, sizeof(struct ahci_tbl));
    
    uint64_t phys_buf = vmm_get_phys((uint64_t)buf);
    
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buf & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)((phys_buf >> 32) & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbc = (count * 512) - 1;
    cmdtbl->prdt_entry[0].i = 1;
    
    struct fis_reg_h2d *cmdfis = (struct fis_reg_h2d*)(&cmdtbl->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_WRITE_DMA_EX;
    
    cmdfis->lba0 = (uint8_t)startl;
    cmdfis->lba1 = (uint8_t)(startl >> 8);
    cmdfis->lba2 = (uint8_t)(startl >> 16);
    cmdfis->device = 1<<6;
    
    cmdfis->lba3 = (uint8_t)(startl >> 24);
    cmdfis->lba4 = (uint8_t)starth;
    cmdfis->lba5 = (uint8_t)(starth >> 8);
    
    cmdfis->countl = count & 0xFF;
    cmdfis->counth = (count >> 8) & 0xFF;
    
    while (port->tfd & (ATA_SR_BSY | ATA_SR_DRQ));
    port->ci |= (1 << slot);
    
    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) return -1;
    }
    
    if (port->is & HBA_PxIS_TFES) return -1;
    
    return 0;
}

static int ahci_identify(struct ahci_device *adev, int port_no, uint64_t *capacity) {
    hba_port_t *port = &adev->abar->ports[port_no];
    
     
    uint16_t *buf = (uint16_t*)kmalloc(512);
    memset(buf, 0, 512);
    uint64_t phys_buf = vmm_get_phys((uint64_t)buf);
    
    int slot = 0;  
    
    struct ahci_cmd_header *cmdheader = (struct ahci_cmd_header*)(adev->port_clb[port_no]);
    cmdheader += slot;
    
    cmdheader->cfl = sizeof(struct fis_reg_h2d)/4;
    cmdheader->w = 0;  
    cmdheader->prdtl = 1;
    
    struct ahci_tbl *cmdtbl = (struct ahci_tbl*)(adev->port_ctba[port_no][slot]);
    memset(cmdtbl, 0, sizeof(struct ahci_tbl));
    
    cmdtbl->prdt_entry[0].dba = (uint32_t)(phys_buf & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbau = (uint32_t)((phys_buf >> 32) & 0xFFFFFFFF);
    cmdtbl->prdt_entry[0].dbc = 511;  
    cmdtbl->prdt_entry[0].i = 1;
    
    struct fis_reg_h2d *cmdfis = (struct fis_reg_h2d*)(&cmdtbl->cfis);
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1;
    cmdfis->command = ATA_CMD_IDENTIFY;
    cmdfis->device = 0;  
    
     
    int timeout = 100000;
    while ((port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && timeout--) {
         
    }
    if (timeout <= 0) {
        kfree(buf);
        return -1;
    }
    
    port->ci |= (1 << slot);
    
     
    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & HBA_PxIS_TFES) {
            kfree(buf);
            return -1;
        }
    }
    
     
    uint64_t sectors = 0;
     
    if (buf[83] & (1 << 10)) {
        sectors = *(uint64_t*)&buf[100];
    } else {
        sectors = *(uint32_t*)&buf[60];
    }
    
    *capacity = sectors;
    
    kfree(buf);
    return 0;
}

 
struct ahci_port_priv {
    struct ahci_device *adev;
    int port_no;
};

 
static void ahci_request(struct request_queue *q) {
    struct request *req;
    while ((req = elv_next_request(q)) != 0) {
        struct gendisk *disk = req->bio->disk;
        struct ahci_port_priv *priv = (struct ahci_port_priv*)disk->private_data;
        
        if (!priv) {
              
             continue;
        }

        struct ahci_device *adev = priv->adev;
        int port_idx = priv->port_no;
        
        hba_port_t *port = &adev->abar->ports[port_idx];
        
        int ret = 0;
        if (req->flags == WRITE) {
            ret = ahci_write(port, req->sector, 0, req->nr_sectors, (uint16_t*)req->buffer);
        } else {
            ret = ahci_read(port, req->sector, 0, req->nr_sectors, (uint16_t*)req->buffer);
        }
        
         
        spinlock_acquire(&q->lock);
        list_del(&req->queuelist);
        INIT_LIST_HEAD(&req->queuelist);
        spinlock_release(&q->lock);
        
         
        kfree(req); 
    }
}

static void port_rebase(hba_port_t *port, int port_no, struct ahci_device *adev) {
    stop_cmd(port);

    void *clb_virt = kmalloc(4096);
    adev->port_clb[port_no] = clb_virt;
    uint64_t clb_phys = vmm_get_phys((uint64_t)clb_virt);
    
    port->clb = (uint32_t)(clb_phys & 0xFFFFFFFF);
    port->clbu = (uint32_t)(clb_phys >> 32);
    memset((void*)clb_virt, 0, 1024);

    void *fb_virt = kmalloc(4096);
    adev->port_fb[port_no] = fb_virt;
    uint64_t fb_phys = vmm_get_phys((uint64_t)fb_virt);
    
    port->fb = (uint32_t)(fb_phys & 0xFFFFFFFF);
    port->fbu = (uint32_t)(fb_phys >> 32);
    memset((void*)fb_virt, 0, 256);

    struct ahci_cmd_header *hdr = (struct ahci_cmd_header*)clb_virt;
    for (int i=0; i<32; i++) {
        hdr[i].prdtl = 8;  
        
        void *ctba_virt = kmalloc(4096);  
        adev->port_ctba[port_no][i] = ctba_virt;
        uint64_t ctba_phys = vmm_get_phys((uint64_t)ctba_virt);
        
        hdr[i].ctba = (uint32_t)(ctba_phys & 0xFFFFFFFF);
        hdr[i].ctbau = (uint32_t)(ctba_phys >> 32);
        memset((void*)ctba_virt, 0, 256);
    }
    
    start_cmd(port);
}

static int ahci_probe(struct device *dev) {
    struct pci_device *pdev = to_pci_device(dev);
    
    if (pdev->class_code != AHCI_PCI_CLASS || pdev->subclass != AHCI_PCI_SUBCLASS) {
        return -1;
    }
    
    pci_enable_device(pdev);
    
    struct ahci_device *adev = (struct ahci_device*)kmalloc(sizeof(struct ahci_device));
    memset(adev, 0, sizeof(struct ahci_device));
    adev->pci_dev = pdev;

    uint64_t abar_phys = (uint64_t)pdev->bar[5] & ~0xF;
    adev->abar = (hba_mem_t*)ioremap(abar_phys, 0x1000); 

    adev->abar->ghc |= AHCI_GHC_AE;
    adev->abar->ghc |= AHCI_GHC_IE;  

    kprint_str("[AHCI] Initialized Controller at BAR5: ");
    kprint_hex((uint64_t)adev->abar);
    kprint_newline();
    
    ahci_devs[ahci_dev_count++] = adev;
    
     
    uint32_t pi = adev->abar->pi;
    for (int i=0; i<32; i++) {
        if (pi & (1 << i)) {
            hba_port_t *port = &adev->abar->ports[i];
            
            int type = check_type(port);
            if (type == 1) {  
                kprint_str("[AHCI] SATA Drive found at port ");
                kprint_hex(i);
                kprint_newline();
                
                port_rebase(port, i, adev);
                
                 
                char name[32];
                 
                name[0] = 's';
                name[1] = 'd';
                name[2] = 'a' + i;  
                name[3] = 0;
                
                register_blkdev(8 + i, name);  
                
                struct gendisk *disk = alloc_disk(1);
                strcpy(disk->disk_name, name);
                disk->major = 8 + i;
                disk->first_minor = 0;
                
                struct ahci_port_priv *priv = (struct ahci_port_priv*)kmalloc(sizeof(struct ahci_port_priv));
                priv->adev = adev;
                priv->port_no = i;
                disk->private_data = priv;
                
                uint64_t capacity_sectors = 0;
                if (ahci_identify(adev, i, &capacity_sectors) == 0) {
                    disk->capacity = capacity_sectors;
                    kprint_str(" AHCI: Capacity: ");
                    kprint_dec(capacity_sectors / 2048); 
                    kprint_str(" MB\n");
                } else {
                    disk->capacity = 0;
                    kprint_str(" AHCI: Identify failed\n");
                }
                
                 
                disk->queue = blk_init_queue(ahci_request, 0);
                
                adev->disk[i] = disk;
                add_disk(disk);
                
            } else if (type == 2) {
                kprint_str("[AHCI] ATAPI Drive found at port ");
                kprint_hex(i);
                kprint_newline();
            }
        }
    }
    
    return 0;
}

static struct pci_driver ahci_driver = {
    .vendor_id = 0xFFFF,  
    .device_id = 0xFFFF,  
    .driver = {
        .name = "ahci",
        .probe = ahci_probe,
    }
};

void ahci_init(void) {
    pci_register_driver(&ahci_driver);
}
