#include "net/arp.h"
#include "net/netdevice.h"
#include "net/skbuff.h"
#include "console.h"
#include "string.h"
#include "net/byteorder.h"
#include "net/ethernet.h"

 
static int arp_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt) {
    (void)pt;
    
    struct etharp_hdr *arp = (struct etharp_hdr *)skb->data;
    
    if (skb->len < sizeof(struct etharp_hdr)) {
        goto drop;
    }
    
    if (arp->ar_hrd != htons(ARPHRD_ETHER)) { 
        goto drop;
    }
    
    if (arp->ar_pro != htons(ETH_P_IP)) { 
        goto drop;
    }
    
    uint16_t op = ntohs(arp->ar_op);
    
    if (op == ARPOP_REQUEST) {

        uint32_t target_ip = arp->ar_tip;  

        if (target_ip == dev->ip_addr) {
             uint32_t sender_ip = arp->ar_sip;

             arp->ar_op = htons(ARPOP_REPLY);

             memcpy(arp->ar_tha, arp->ar_sha, 6);
             arp->ar_tip = sender_ip;

             memcpy(arp->ar_sha, dev->dev_addr, 6);
             arp->ar_sip = dev->ip_addr;

             struct ethhdr *eth = (struct ethhdr *)(skb->data - sizeof(struct ethhdr));
             memcpy(eth->h_dest, eth->h_source, 6);
             memcpy(eth->h_source, dev->dev_addr, 6);

             if (dev->netdev_ops->start_xmit) {
                 dev->netdev_ops->start_xmit(skb, dev);
                 return 0;
             }
        }
        
    } else if (op == ARPOP_REPLY) {}
    
    kfree_skb(skb);
    return 0;

drop:
    kfree_skb(skb);
    return 0;
}

static struct packet_type arp_packet_type = {
    .type = 0x0608,  
    .func = arp_rcv,
};

void arp_init() {
    dev_add_pack(&arp_packet_type);
    kprint_str("ARP: Initialized\n");
}
