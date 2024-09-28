#ifndef DHCP_PHASES_H
#define DHCP_PHASES_H

#include <netinet/in.h>
#include "dhcp_utils.h"

typedef struct {
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
    uint8_t options[312];
} DHCPPacket;

void handle_dhcp_discover(DHCPPacket *packet, struct sockaddr_in *client_addr);
void handle_dhcp_request(DHCPPacket *packet, struct sockaddr_in *client_addr);
void handle_dhcp_renew(DHCPPacket *packet, struct sockaddr_in *client_addr);
void handle_remote_request(DHCPPacket *packet, struct sockaddr_in *client_addr);

#endif // DHCP_PHASES_H