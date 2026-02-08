#ifndef _NET_SKBUFF_H
#define _NET_SKBUFF_H

#include <stdint.h>
#include "list.h"

struct net_device;

struct sk_buff {
    struct list_head list;       
    struct net_device *dev;      
    
    uint16_t protocol;           
    uint8_t packet_type;         
    uint32_t len;                
    uint32_t true_size;          
    
    uint8_t *mac_header;
    uint8_t *network_header;
    uint8_t *transport_header;
    
     
    uint8_t *head;               
    uint8_t *data;               
    uint8_t *tail;               
    uint8_t *end;                
    
     
    uint8_t cb[48];
};

 
struct sk_buff *alloc_skb(uint32_t size);
void kfree_skb(struct sk_buff *skb);

 
uint8_t *skb_put(struct sk_buff *skb, uint32_t len);       
uint8_t *skb_push(struct sk_buff *skb, uint32_t len);      
uint8_t *skb_pull(struct sk_buff *skb, uint32_t len);      
void skb_reserve(struct sk_buff *skb, uint32_t len);       

#endif  
