#include "net/dhcp.h"
#include "net/udp.h"
#include "net/ip.h"
#include "net/byteorder.h"
#include "console.h"
#include "string.h"

void dhcp_discover(struct net_device *dev) {
    struct dhcp_packet packet;
    memset(&packet, 0, sizeof(packet));
    
    packet.op = 1;  
    packet.htype = 1;  
    packet.hlen = 6;
    packet.xid = htonl(0x12345678);  
    packet.flags = htons(0x8000);  
    packet.cookie = htonl(0x63825363);  
    
    memcpy(packet.chaddr, dev->dev_addr, 6);
    
    int offset = 0;
    
    packet.options[offset++] = 53;
    packet.options[offset++] = 1;
    packet.options[offset++] = 1;
    
    packet.options[offset++] = 55;
    packet.options[offset++] = 3;
    packet.options[offset++] = 1;  
    packet.options[offset++] = 3;  
    packet.options[offset++] = 6;  
    
    packet.options[offset++] = 255;
    
    kprint_str("DHCP: Sending DISCOVER on "); kprint_str(dev->name); kprint_newline();
    
    extern int udp_send_packet(struct net_device *dev, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, void *data, uint32_t len);
    
    udp_send_packet(dev, 0xFFFFFFFF, 68, 67, &packet, sizeof(packet));
}

void dhcp_input(struct sk_buff *skb) {
    struct dhcp_packet *packet = (struct dhcp_packet *)(skb->data + sizeof(struct iphdr) + sizeof(struct udphdr));
    
    if (ntohl(packet->cookie) != 0x63825363) return;
    
    if (packet->op != 2) return;
    
    struct net_device *dev = skb->dev;
    
    uint8_t *options = packet->options;
    uint32_t my_mask = 0;
    uint32_t my_gw = 0;
    uint32_t my_dns = 0;
    uint8_t msg_type = 0;
    
    int i = 0;
    while (i < 308) {
        uint8_t opt = options[i++];
        if (opt == 255) break;  
        if (opt == 0) continue;  
        
        uint8_t len = options[i++];
        
        if (opt == 53) {  
            if (i < 308) msg_type = options[i];
        } else if (opt == 1) {  
            if (i + 4 <= 308) memcpy(&my_mask, &options[i], 4);
        } else if (opt == 3) {  
             if (i + 4 <= 308) memcpy(&my_gw, &options[i], 4);
        } else if (opt == 6) {  
             if (i + 4 <= 308) memcpy(&my_dns, &options[i], 4);
        }
        
        i += len;
    }

    if (msg_type == 2 || msg_type == 5) {  
         dev->ip_addr = packet->yiaddr;
         dev->netmask = my_mask;
         dev->gateway = my_gw;
         dev->dns = my_dns;
         
         kprint_str("DHCP: Bound to IP ");
         kprint_dec(dev->ip_addr & 0xFF); kprint_str(".");
         kprint_dec((dev->ip_addr >> 8) & 0xFF); kprint_str(".");
         kprint_dec((dev->ip_addr >> 16) & 0xFF); kprint_str(".");
         kprint_dec((dev->ip_addr >> 24) & 0xFF);
         kprint_newline();
         
         if (msg_type == 2) {
             kprint_str("DHCP: Note - Accepted OFFER (Request skipped)\n");
         }
    }
}
