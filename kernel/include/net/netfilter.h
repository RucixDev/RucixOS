#ifndef _NET_NETFILTER_H
#define _NET_NETFILTER_H

#include "net/skbuff.h"
#include "net/netdevice.h"

struct net;

 
enum nf_inet_hooks {
    NF_INET_PRE_ROUTING,
    NF_INET_LOCAL_IN,
    NF_INET_FORWARD,
    NF_INET_LOCAL_OUT,
    NF_INET_POST_ROUTING,
    NF_INET_NUMHOOKS
};

 
#define NF_DROP 0
#define NF_ACCEPT 1
#define NF_STOLEN 2
#define NF_QUEUE 3
#define NF_REPEAT 4
#define NF_STOP 5

typedef unsigned int nf_hookfn(void *priv,
                               struct sk_buff *skb,
                               const struct net_device *in,
                               const struct net_device *out,
                               int (*okfn)(struct sk_buff *));

int nf_register_hook(struct net *net, enum nf_inet_hooks hook, nf_hookfn *fn);
int nf_unregister_hook(struct net *net, enum nf_inet_hooks hook, nf_hookfn *fn);
int nf_hook(int pf, unsigned int hook, struct net *net, struct sk_buff *skb,
            struct net_device *in, struct net_device *out,
            int (*okfn)(struct sk_buff *));

#endif  
