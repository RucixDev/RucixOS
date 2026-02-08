#include "net/wireless.h"
#include "drivers/pci.h"
#include "console.h"
#include "heap.h"
#include "io.h"

#define ATHEROS_VENDOR_ID 0x168c
#define AR5212_DEVICE_ID  0x0013

struct ath5k_softc {
    struct wireless_dev *wdev;
    struct pci_device *pdev;
    uint64_t mmio_base;
    uint64_t mmio_len;
};

static int ath5k_scan(struct wireless_dev *wdev) {
    kprint_str("ath5k: Scanning...\n");
    return 0;
}

static int ath5k_probe(struct pci_device *pdev) {
    kprint_str("ath5k: Found Atheros WiFi at ");
    kprint_hex(pdev->bus); kprint_str(":"); kprint_hex(pdev->slot); kprint_newline();
    
    struct ath5k_softc *sc = (struct ath5k_softc*)kmalloc(sizeof(struct ath5k_softc));
    struct wireless_dev *wdev = alloc_wireless_dev(0, "wlan0");
    
    if (!wdev) return -1;
    
    wdev->scan = ath5k_scan;
    
    sc->wdev = wdev;
    sc->pdev = pdev;
    
    uint64_t mmio_phys = (uint64_t)pdev->bar[0] & ~0xF;
     
    register_wireless_dev(wdev);
    
    return 0;
}

static struct pci_driver ath5k_driver = {
    .vendor_id = ATHEROS_VENDOR_ID,
    .device_id = AR5212_DEVICE_ID,
    .driver = {
        .name = "ath5k",
         
    }
};

static int ath5k_probe_wrapper(struct device *dev) {
    struct pci_device *pdev = to_pci_device(dev);
    return ath5k_probe(pdev);
}

void ath5k_init(void) {
    ath5k_driver.driver.probe = ath5k_probe_wrapper;
    pci_register_driver(&ath5k_driver);
}
