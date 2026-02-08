#include "net/icmp.h"
#include "net/ip.h"
#include "console.h"
#include "string.h"

static uint16_t checksum(uint16_t *addr, int count) {
    register uint32_t sum = 0;
    while (count > 1) {
        sum += *addr++;
        count -= 2;
    }
    if (count > 0)
        sum += *(uint8_t *)addr;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

void icmp_reply(struct sk_buff *skb, struct icmphdr *icmph) {
    struct iphdr *iph = (struct iphdr *)skb->network_header;

    uint32_t daddr = iph->saddr;
    uint32_t saddr = iph->daddr;

    iph->daddr = daddr;
    iph->saddr = saddr;

    icmph->type = ICMP_ECHOREPLY;
    icmph->checksum = 0;
    icmph->checksum = checksum((uint16_t *)icmph, skb->len - sizeof(struct iphdr));
   
    iph->check = 0;
    iph->check = checksum((uint16_t *)iph, sizeof(struct iphdr));
    
    kprint_str("ICMP: Sending Echo Reply\n");
    
    if (skb->dev && skb->dev->netdev_ops->start_xmit) {
        skb->dev->netdev_ops->start_xmit(skb, skb->dev);
    } else {
        kfree_skb(skb);
    }
}

void icmp_rcv(struct sk_buff *skb) {
    struct icmphdr *icmph = (struct icmphdr *)(skb->data + sizeof(struct iphdr));
  
    switch (icmph->type) {
        case ICMP_ECHO:
            icmp_reply(skb, icmph);
            return;  
        default:
            break;
    }
    
    kfree_skb(skb);
}

void icmp_init() {
    kprint_str("ICMP: Initialized\n");
}
