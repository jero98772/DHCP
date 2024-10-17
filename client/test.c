#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "0.0.0.0"
#define DHCP_SERVER_PORT 667
#define DHCP_CLIENT_PORT 668
#define MAX_DHCP_PACKET_SIZE 1024

#define MAX_DOMAIN_LENGTH 256
#define MAX_IP_LENGTH 16

typedef struct {
    char domain[MAX_DOMAIN_LENGTH];
    char ip[MAX_IP_LENGTH];
} DNSQuery;

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

void send_dhcp_discover(int sock, struct sockaddr_in *server_addr) {
    DHCPPacket packet;
    memset(&packet, 0, sizeof(DHCPPacket));

    packet.op = 1; // BOOTREQUEST
    packet.htype = 1; // Ethernet
    packet.hlen = 6; // MAC address length
    packet.xid = random(); // Random transaction ID

    // Set DHCP message type to DHCPDISCOVER
    packet.options[0] = 53; // DHCP Message Type option
    packet.options[1] = 1;  // Length
    packet.options[2] = 1;  // DHCPDISCOVER

    if (sendto(sock, &packet, sizeof(DHCPPacket), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("DHCP Discover sendto failed");
    } else {
        printf("Sent DHCP Discover\n");
    }
}

void send_dns_query(int sock, struct sockaddr_in *server_addr, const char *domain) {
    DNSQuery query;
    strncpy(query.domain, domain, MAX_DOMAIN_LENGTH - 1);
    query.domain[MAX_DOMAIN_LENGTH - 1] = '\0';

    if (sendto(sock, &query, sizeof(DNSQuery), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("DNS query sendto failed");
    } else {
        printf("Sent DNS query for domain: %s\n", query.domain);
    }
}

int main() {
    int dhcp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    int dns_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (dhcp_sock < 0 || dns_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    // Enable SO_REUSEADDR to reuse the address and port
    int opt = 1;
    if (setsockopt(dhcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(dhcp_sock);
        close(dns_sock);
        exit(1);
    }
    struct sockaddr_in dhcp_server_addr, dns_server_addr, client_addr;
    memset(&dhcp_server_addr, 0, sizeof(dhcp_server_addr));
    memset(&dns_server_addr, 0, sizeof(dns_server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    // DHCP server address
    dhcp_server_addr.sin_family = AF_INET;
    dhcp_server_addr.sin_port = htons(DHCP_SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &dhcp_server_addr.sin_addr);

    // DNS server address (using the same IP but different port)
    dns_server_addr.sin_family = AF_INET;
    dns_server_addr.sin_port = htons(DHCP_SERVER_PORT); // Using the same port for DNS
    inet_pton(AF_INET, SERVER_IP, &dns_server_addr.sin_addr);

    // Client address for binding
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(DHCP_CLIENT_PORT);

    // Bind the DHCP socket
    if (bind(dhcp_sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("DHCP socket bind failed");
        close(dhcp_sock);
        close(dns_sock);
        exit(1);
    }

    // Send DHCP Discover
    send_dhcp_discover(dhcp_sock, &dhcp_server_addr);

    // Send DNS query (example.com)
    send_dns_query(dns_sock, &dns_server_addr, "example.com");

    // Receive and handle responses
    char buffer[MAX_DHCP_PACKET_SIZE];
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);

    while (1) {
        ssize_t received = recvfrom(dhcp_sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &server_len);
        if (received < 0) {
            perror("recvfrom failed");
            continue;
        }

        if (received == sizeof(DHCPPacket)) {
            DHCPPacket *dhcp_response = (DHCPPacket *)buffer;
            printf("Received DHCP packet (type: %d)\n", dhcp_response->options[2]);
            // Handle DHCP response here
        } else if (received == sizeof(DNSQuery)) {
            DNSQuery *dns_response = (DNSQuery *)buffer;
            printf("Received DNS response:\n");
            printf("Domain: %s\n", dns_response->domain);
            printf("IP Address: %s\n", dns_response->ip);
        } else {
            printf("Received unknown packet type (size: %zd)\n", received);
        }
    }

    close(dhcp_sock);
    close(dns_sock);
    return 0;
}
