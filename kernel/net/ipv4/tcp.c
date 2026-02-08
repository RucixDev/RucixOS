#include "net/tcp.h"
#include "net/ip.h"
#include "console.h"
#include "string.h"

 
int tcp_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt) {
    (void)dev;
    (void)pt;
    struct tcphdr *th = (struct tcphdr *)(skb->data + sizeof(struct iphdr));
    (void)th;

    if (skb->len < sizeof(struct iphdr) + sizeof(struct tcphdr)) {
        goto drop;
    }

    kfree_skb(skb);
    return 0;

drop:
    kfree_skb(skb);
    return 0;
}

void tcp_init() {
    kprint_str("TCP: Initialized\n");
}
