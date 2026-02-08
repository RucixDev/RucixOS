#ifndef _NET_ARP_H
#define _NET_ARP_H

#include <stdint.h>
#include "net/ethernet.h"
#include "net/netdevice.h"

 
struct arphdr {
    uint16_t ar_hrd;     
    uint16_t ar_pro;     
    uint8_t  ar_hln;     
    uint8_t  ar_pln;     
    uint16_t ar_op;      

     
     
} __attribute__((packed));

#define ARPHRD_ETHER    1
#define ARPOP_REQUEST   1
#define ARPOP_REPLY     2

 
struct etharp_hdr {
    uint16_t ar_hrd;
    uint16_t ar_pro;
    uint8_t  ar_hln;
    uint8_t  ar_pln;
    uint16_t ar_op;
    
    uint8_t  ar_sha[ETH_ALEN];  
    uint32_t ar_sip;            
    uint8_t  ar_tha[ETH_ALEN];  
    uint32_t ar_tip;            
} __attribute__((packed));

void arp_init(void);
void arp_send(uint16_t op, uint32_t dest_ip, uint8_t *dest_mac, uint32_t src_ip, struct net_device *dev);

#endif  
