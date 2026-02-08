#ifndef _DRIVERS_AHCI_H
#define _DRIVERS_AHCI_H

#include "types.h"
#include "drivers/pci.h"
#include "drivers/blockdev.h"

#define AHCI_PCI_CLASS 0x01
#define AHCI_PCI_SUBCLASS 0x06
#define AHCI_PCI_PROG_IF 0x01

#define HBA_MEM_CAP     0x00
#define HBA_MEM_GHC     0x04
#define HBA_MEM_IS      0x08
#define HBA_MEM_PI      0x0C
#define HBA_MEM_VS      0x10

#define HBA_PxCLB       0x00
#define HBA_PxCLBU      0x04
#define HBA_PxFB        0x08
#define HBA_PxFBU       0x0C
#define HBA_PxIS        0x10
#define HBA_PxIE        0x14
#define HBA_PxCMD       0x18
#define HBA_PxTFD       0x20
#define HBA_PxSIG       0x24
#define HBA_PxSSTS      0x28
#define HBA_PxSCTL      0x2C
#define HBA_PxSERR      0x30
#define HBA_PxSACT      0x34
#define HBA_PxCI        0x38

#define AHCI_GHC_AE     (1 << 31)   
#define AHCI_GHC_IR     (1 << 1)    
#define AHCI_GHC_IE     (1 << 1)    
#define AHCI_GHC_HR     (1 << 0)    

#define AHCI_PxCMD_ST   (1 << 0)    
#define AHCI_PxCMD_SUD  (1 << 1)    
#define AHCI_PxCMD_POD  (1 << 2)    
#define AHCI_PxCMD_CLO  (1 << 3)    
#define AHCI_PxCMD_FRE  (1 << 4)    
#define AHCI_PxCMD_FR   (1 << 14)   
#define AHCI_PxCMD_CR   (1 << 15)   

#define HBA_PxIS_TFES  (1 << 30)   

typedef enum {
    FIS_TYPE_REG_H2D    = 0x27,
    FIS_TYPE_REG_D2H    = 0x34,
    FIS_TYPE_DMA_ACT    = 0x39,
    FIS_TYPE_DMA_SETUP  = 0x41,
    FIS_TYPE_DATA       = 0x46,
    FIS_TYPE_BIST       = 0x58,
    FIS_TYPE_PIO_SETUP  = 0x5F,
    FIS_TYPE_DEV_BITS   = 0xA1,
} FIS_TYPE;

struct fis_reg_h2d {
    uint8_t  fis_type;
    uint8_t  pmport : 4;
    uint8_t  rsv0   : 3;
    uint8_t  c      : 1;   
    uint8_t  command;      
    uint8_t  featurel;     
    uint8_t  lba0;         
    uint8_t  lba1;         
    uint8_t  lba2;         
    uint8_t  device;       
    uint8_t  lba3;         
    uint8_t  lba4;         
    uint8_t  lba5;         
    uint8_t  featureh;     
    uint8_t  countl;       
    uint8_t  counth;       
    uint8_t  icc;          
    uint8_t  control;      
    uint8_t  rsv1[4];
} __attribute__((packed));

struct fis_reg_d2h {
    uint8_t  fis_type;
    uint8_t  pmport : 4;
    uint8_t  rsv0   : 2;
    uint8_t  i      : 1;   
    uint8_t  rsv1   : 1;
    uint8_t  status;       
    uint8_t  error;        
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  rsv2;
    uint8_t  countl;
    uint8_t  counth;
    uint8_t  rsv3[2];
    uint8_t  rsv4[4];
} __attribute__((packed));

struct ahci_cmd_header {
    uint8_t  cfl:5;     
    uint8_t  a:1;       
    uint8_t  w:1;       
    uint8_t  p:1;       
    uint8_t  r:1;       
    uint8_t  b:1;       
    uint8_t  c:1;       
    uint8_t  rsv0:1;
    uint8_t  pmp:4;     
    uint16_t prdtl;     
    volatile uint32_t prdbc;  
    uint32_t ctba;      
    uint32_t ctbau;     
    uint32_t rsv1[4];
} __attribute__((packed));

struct ahci_prdt_entry {
    uint32_t dba;       
    uint32_t dbau;      
    uint32_t rsv0;
    uint32_t dbc:22;    
    uint32_t rsv1:9;
    uint32_t i:1;       
} __attribute__((packed));

struct ahci_tbl {
    uint8_t  cfis[64];  
    uint8_t  acmd[16];  
    uint8_t  rsv[48];   
    struct ahci_prdt_entry prdt_entry[1];  
} __attribute__((packed));

typedef volatile struct tag_hba_port {
    uint32_t clb;       
    uint32_t clbu;      
    uint32_t fb;        
    uint32_t fbu;       
    uint32_t is;        
    uint32_t ie;        
    uint32_t cmd;       
    uint32_t rsv0;
    uint32_t tfd;       
    uint32_t sig;       
    uint32_t ssts;      
    uint32_t sctl;      
    uint32_t serr;      
    uint32_t sact;      
    uint32_t ci;        
    uint32_t sntf;      
    uint32_t fbs;       
    uint32_t rsv1[11];
    uint32_t vendor[4];
} hba_port_t;

typedef volatile struct tag_hba_mem {
    uint32_t cap;       
    uint32_t ghc;       
    uint32_t is;        
    uint32_t pi;        
    uint32_t vs;        
    uint32_t ccc_ctl;   
    uint32_t ccc_pts;   
    uint32_t em_loc;    
    uint32_t em_ctl;    
    uint32_t cap2;      
    uint32_t bohc;      
    uint8_t  rsv[0xA0-0x2C];
    uint8_t  vendor[0x100-0xA0];
    hba_port_t ports[1];  
} hba_mem_t;

#define ATA_CMD_READ_DMA_EX     0x25
#define ATA_CMD_WRITE_DMA_EX    0x35
#define ATA_CMD_IDENTIFY        0xEC

#define ATA_SR_BSY      0x80
#define ATA_SR_DRQ      0x08
#define ATA_SR_ERR      0x01
 
struct ahci_device {
    struct pci_device *pci_dev;
    hba_mem_t *abar;   
    
    struct gendisk *disk[32];  

    void *port_clb[32]; 
    void *port_fb[32];
    void *port_ctba[32][32];  
};

void ahci_init(void);

#endif
