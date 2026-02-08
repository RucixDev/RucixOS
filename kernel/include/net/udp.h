#ifndef _NET_UDP_H
#define _NET_UDP_H

#include <stdint.h>
#include "net/skbuff.h"
#include "net/socket.h"
#include "net/netdevice.h"

struct udphdr {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
} __attribute__((packed));

void udp_init(void);
int udp_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt);

#endif  
