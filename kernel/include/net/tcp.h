#ifndef _NET_TCP_H
#define _NET_TCP_H

#include <stdint.h>
#include "net/skbuff.h"
#include "net/socket.h"
#include "net/netdevice.h"

 
struct tcphdr {
    uint16_t source;
    uint16_t dest;
    uint32_t seq;
    uint32_t ack_seq;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint16_t res1:4;
    uint16_t doff:4;
    uint16_t fin:1;
    uint16_t syn:1;
    uint16_t rst:1;
    uint16_t psh:1;
    uint16_t ack:1;
    uint16_t urg:1;
    uint16_t ece:1;
    uint16_t cwr:1;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint16_t doff:4;
    uint16_t res1:4;
    uint16_t cwr:1;
    uint16_t ece:1;
    uint16_t urg:1;
    uint16_t ack:1;
    uint16_t psh:1;
    uint16_t rst:1;
    uint16_t syn:1;
    uint16_t fin:1;
#else
#error "Unknown byte order"
#endif
    uint16_t window;
    uint16_t check;
    uint16_t urg_ptr;
} __attribute__((packed));

 
enum {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING,
    TCP_NEW_SYN_RECV,
    TCP_MAX_STATES
};

void tcp_init(void);
int tcp_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt);

#endif  
