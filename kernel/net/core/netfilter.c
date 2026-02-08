#include "net/netfilter.h"
#include "console.h"

int nf_register_hook(struct net *net, enum nf_inet_hooks hook, nf_hookfn *fn) {
    (void)net; (void)hook; (void)fn;
    return 0;
}

int nf_unregister_hook(struct net *net, enum nf_inet_hooks hook, nf_hookfn *fn) {
    (void)net; (void)hook; (void)fn;
    return 0;
}

int nf_hook(int pf, unsigned int hook, struct net *net, struct sk_buff *skb,
            struct net_device *in, struct net_device *out,
            int (*okfn)(struct sk_buff *)) {
    (void)pf; (void)hook; (void)net; (void)in; (void)out;
     
     
    return okfn(skb);
}
