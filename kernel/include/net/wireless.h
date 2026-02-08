#ifndef WIRELESS_H
#define WIRELESS_H

#include "net/netdevice.h"

 
#define IEEE80211_TYPE_MGT  0
#define IEEE80211_TYPE_CTL  1
#define IEEE80211_TYPE_DATA 2

 
#define IEEE80211_STYPE_BEACON 0x08
#define IEEE80211_STYPE_PROBE_REQ 0x04
#define IEEE80211_STYPE_PROBE_RESP 0x05
#define IEEE80211_STYPE_ASSOC_REQ 0x00
#define IEEE80211_STYPE_ASSOC_RESP 0x01
#define IEEE80211_STYPE_AUTH 0x0B
#define IEEE80211_STYPE_DEAUTH 0x0C

struct wireless_dev {
    struct net_device *netdev;
    
    char ssid[33];
    uint8_t bssid[6];
    int channel;
    int mode;  
    
     
    uint32_t tx_packets;
    uint32_t rx_packets;
    int signal_strength;  
    
     
    int (*scan)(struct wireless_dev *wdev);
    int (*associate)(struct wireless_dev *wdev, const char *ssid, const char *key);
    int (*disassociate)(struct wireless_dev *wdev);
};

struct wireless_dev *alloc_wireless_dev(int sizeof_priv, const char *name);
int register_wireless_dev(struct wireless_dev *wdev);
int unregister_wireless_dev(struct wireless_dev *wdev);

#endif
