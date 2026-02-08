#ifndef _NET_DHCP_H
#define _NET_DHCP_H

#include <stdint.h>
#include "net/netdevice.h"
#include "net/skbuff.h"

struct dhcp_packet {
    uint8_t op;       
    uint8_t htype;    
    uint8_t hlen;     
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t cookie;
    uint8_t options[308];
} __attribute__((packed));

void dhcp_discover(struct net_device *dev);
void dhcp_input(struct sk_buff *skb);

#endif
