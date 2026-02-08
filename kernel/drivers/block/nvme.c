#include "drivers/nvme.h"
#include "drivers/pci.h"
#include "drivers/blockdev.h"
#include "vmm.h"
#include "pmm.h"
#include "heap.h"
#include "console.h"
#include "string.h"
#include "io.h"

#define NVME_VENDOR_ID 0x8086  

struct nvme_queue {
    uint16_t id;
    uint16_t q_depth;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint16_t vector;
    
    struct nvme_sqe *sq_cmds;
    struct nvme_cqe *cq_es;
    
    uint64_t sq_dma;
    uint64_t cq_dma;
    
    uint32_t *db_sq;  
    uint32_t *db_cq;
};

struct nvme_ctrl {
    struct pci_device *pdev;
    uint64_t mmio_base;
    uint64_t mmio_len;
    
    uint32_t db_stride;
    uint64_t cap;
    
    struct nvme_queue admin_q;
    struct nvme_queue *io_queues;
    int num_io_queues;
    
    struct nvme_id_ns *namespaces;
    int num_namespaces;
};

static uint32_t nvme_read32(struct nvme_ctrl *ctrl, uint32_t offset) {
    return *(volatile uint32_t*)(ctrl->mmio_base + offset);
}

static uint64_t nvme_read64(struct nvme_ctrl *ctrl, uint32_t offset) {
    return *(volatile uint64_t*)(ctrl->mmio_base + offset);
}

static void nvme_write32(struct nvme_ctrl *ctrl, uint32_t offset, uint32_t val) {
    *(volatile uint32_t*)(ctrl->mmio_base + offset) = val;
}

static void nvme_write64(struct nvme_ctrl *ctrl, uint32_t offset, uint64_t val) {
    *(volatile uint64_t*)(ctrl->mmio_base + offset) = val;
}

static int nvme_wait_ready(struct nvme_ctrl *ctrl, int ready) {
    int timeout = 10000;  
    while (timeout > 0) {
        uint32_t csts = nvme_read32(ctrl, NVME_REG_CSTS);
        if (!!(csts & NVME_CSTS_RDY) == ready) return 0;
         
        timeout--;
    }
    return -1;
}

static int nvme_reset_controller(struct nvme_ctrl *ctrl) {
     
    uint32_t cc = nvme_read32(ctrl, NVME_REG_CC);
    if (cc & NVME_CC_EN) {
        cc &= ~NVME_CC_EN;
        nvme_write32(ctrl, NVME_REG_CC, cc);
        if (nvme_wait_ready(ctrl, 0) != 0) {
            kprint_str("NVMe: Failed to disable controller\n");
            return -1;
        }
    }
    return 0;
}

static int nvme_init_queue(struct nvme_ctrl *ctrl, struct nvme_queue *q, int id, int depth) {
    q->id = id;
    q->q_depth = depth;
    q->sq_tail = 0;
    q->cq_head = 0;
    
     
     
    size_t sq_size = depth * sizeof(struct nvme_sqe);
    size_t cq_size = depth * sizeof(struct nvme_cqe);
    
    q->sq_cmds = (struct nvme_sqe*)kmalloc(sq_size);
    q->cq_es = (struct nvme_cqe*)kmalloc(cq_size);
    
    memset(q->sq_cmds, 0, sq_size);
    memset(q->cq_es, 0, cq_size);
    
    q->sq_dma = vmm_get_phys((uint64_t)q->sq_cmds);
    q->cq_dma = vmm_get_phys((uint64_t)q->cq_es);
    
     
     
    q->db_sq = (uint32_t*)(ctrl->mmio_base + 0x1000 + (2 * id * ctrl->db_stride));
    q->db_cq = (uint32_t*)(ctrl->mmio_base + 0x1000 + ((2 * id + 1) * ctrl->db_stride));
    
    return 0;
}

static int nvme_probe(struct pci_device *pdev) {
    kprint_str("NVMe: Found controller at ");
    kprint_hex(pdev->bus); kprint_str(":"); kprint_hex(pdev->slot); kprint_str("\n");
    
     
    uint16_t cmd = pci_read_word(pdev->bus, pdev->slot, pdev->func, PCI_COMMAND);
    cmd |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;
    pci_write_word(pdev->bus, pdev->slot, pdev->func, PCI_COMMAND, cmd);
    
    struct nvme_ctrl *ctrl = (struct nvme_ctrl*)kmalloc(sizeof(struct nvme_ctrl));
    memset(ctrl, 0, sizeof(struct nvme_ctrl));
    ctrl->pdev = pdev;
    
     
    uint64_t mmio_phys = (uint64_t)pdev->bar[0] & ~0xF;
    uint64_t mmio_len = 8192;  
    
    ctrl->mmio_base = (uint64_t)ioremap(mmio_phys, mmio_len);
    ctrl->mmio_len = mmio_len;
    
     
    ctrl->cap = nvme_read64(ctrl, NVME_REG_CAP);
    int dstrd = (ctrl->cap >> 32) & 0xF;
    ctrl->db_stride = 1 << (2 + dstrd);  
     
    ctrl->db_stride = 1 << (2 + dstrd);
    
     
    nvme_reset_controller(ctrl);
    
     
    nvme_init_queue(ctrl, &ctrl->admin_q, 0, 64);  
    
     
    nvme_write32(ctrl, NVME_REG_AQA, (63 << 16) | 63);  
    nvme_write64(ctrl, NVME_REG_ASQ, ctrl->admin_q.sq_dma);
    nvme_write64(ctrl, NVME_REG_ACQ, ctrl->admin_q.cq_dma);
    
     
    uint32_t cc = nvme_read32(ctrl, NVME_REG_CC);
    cc |= NVME_CC_IOSQES | NVME_CC_IOCQES;  
    cc |= NVME_CC_EN;
    cc |= NVME_CC_CSS_NVM;
    cc |= NVME_CC_MPS_4K;
    nvme_write32(ctrl, NVME_REG_CC, cc);
    
    if (nvme_wait_ready(ctrl, 1) != 0) {
        kprint_str("NVMe: Failed to enable controller\n");
        return -1;
    }
    
    kprint_str("NVMe: Controller Enabled\n");
    
     
    
    return 0;
}

static struct pci_driver nvme_driver = {
    .class = 0x01,  
    .subclass = 0x08,  
    .driver = {
        .name = "nvme",
        .probe = (int(*)(struct device*))nvme_probe,  
         
    }
};

static int nvme_probe_wrapper(struct device *dev) {
    struct pci_device *pdev = to_pci_device(dev);
    return nvme_probe(pdev);
}

void nvme_init() {
    nvme_driver.driver.probe = nvme_probe_wrapper;
    pci_register_driver(&nvme_driver);
}
