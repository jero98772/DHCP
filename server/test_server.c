#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_CLIENTS 100
#define IP_POOL_START "192.168.1.100"
#define IP_POOL_END "192.168.1.200"
#define SERVER_IP "0.0.0.0"
//we use 6767 because is used by real dhcp
#define DHCP_SERVER_PORT 667
//we use 6868 because is used by real dhcp
#define DHCP_CLIENT_PORT 668
#define MAX_DHCP_PACKET_SIZE 1024
#define LOG_FILE "dhcp_server.log"

FILE *log_file = NULL;

// Mutex for thread-safe logging
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lease_mutex = PTHREAD_MUTEX_INITIALIZER;

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

typedef struct {
    char ip[16];
    char mac[18];
    time_t lease_start;
    int lease_time;
    int state; // 0: free, 1: offered, 2: leased
} IPLease;

IPLease ip_leases[MAX_CLIENTS];
int num_leases = 0;


// DNS HashMap (simplified)
#define MAX_DNS_ENTRIES 100

typedef struct {
    char domain[256];
    char ip[16];
} DNSEntry;

DNSEntry dns_table[MAX_DNS_ENTRIES];
int dns_entries = 0;
void add_dhcp_option(uint8_t *options, int *offset, uint8_t option_code, uint8_t option_length, uint8_t *option_value) {
    printf("Adding DHCP option: Code %d, Length %d\n", option_code, option_length);
    options[(*offset)++] = option_code;
    options[(*offset)++] = option_length;
    memcpy(&options[*offset], option_value, option_length);
    *offset += option_length;
}

void add_dns_entry(const char* domain, const char* ip) {
    if (dns_entries < MAX_DNS_ENTRIES) {
        strncpy(dns_table[dns_entries].domain, domain, 255);
        strncpy(dns_table[dns_entries].ip, ip, 15);
        dns_entries++;
        printf("Added DNS entry: %s -> %s\n", domain, ip);
    }
}

const char* lookup_dns(const char* domain) {
    for (int i = 0; i < dns_entries; i++) {
        if (strcmp(dns_table[i].domain, domain) == 0) {
            printf("Found DNS entry for %s: %s\n", domain, dns_table[i].ip);
            return dns_table[i].ip;
        }
    }
    printf("No DNS entry found for %s\n", domain);
    return NULL;
}

void init_log() {
    log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Error opening log file");
        exit(1);
    }
}

void close_log() {
    if (log_file != NULL) {
        fclose(log_file);
    }
}

void write_log(const char *message) {
    if (log_file == NULL) {
        return;
    }

    time_t now;
    time(&now);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0'; // Remove newline

    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "[%s] %s\n", date, message);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}

uint32_t get_next_available_ip() {
    uint32_t start_ip = inet_addr(IP_POOL_START);
    uint32_t end_ip = inet_addr(IP_POOL_END);

    for (uint32_t ip = ntohl(start_ip); ip <= ntohl(end_ip); ip++) {
        char ip_str[16];
        struct in_addr addr;
        addr.s_addr = htonl(ip);
        inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));

        int available = 1;
        for (int i = 0; i < num_leases; i++) {
            if (strcmp(ip_leases[i].ip, ip_str) == 0 && ip_leases[i].state != 0) {
                available = 0;
                break;
            }
        }

        if (available) {
            return htonl(ip);
        }
    }

    write_log("Error: IP address pool exhausted");
    return 0; // No available IPs
}

// Implement lease renewal
void handle_dhcp_renew(DHCPPacket *packet, struct sockaddr_in *client_addr) {
    write_log("Handling DHCP renew request");

    // Format client MAC address as a string
    char client_mac[18];
    snprintf(client_mac, sizeof(client_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             packet->chaddr[0], packet->chaddr[1], packet->chaddr[2],
             packet->chaddr[3], packet->chaddr[4], packet->chaddr[5]);

    pthread_mutex_lock(&lease_mutex);
    for (int i = 0; i < num_leases; i++) {
        if (strcmp(ip_leases[i].mac, client_mac) == 0) {
            // Renew the lease
            ip_leases[i].lease_start = time(NULL);

            // Prepare the DHCP ACK response
            DHCPPacket response;
            memset(&response, 0, sizeof(DHCPPacket));

            response.op = 2; // Boot Reply
            response.htype = packet->htype;
            response.hlen = packet->hlen;
            response.xid = packet->xid;
            response.yiaddr = inet_addr(ip_leases[i].ip); // Client's IP address
            response.siaddr = inet_addr(SERVER_IP); // Server IP address
            memcpy(response.chaddr, packet->chaddr, 16); // Client MAC address

            // Add DHCP options
            int option_offset = 0;
            uint8_t dhcp_msg_type = 5; // DHCP ACK
            add_dhcp_option(response.options, &option_offset, 53, 1, &dhcp_msg_type);

            uint32_t lease_time = htonl(ip_leases[i].lease_time);
            add_dhcp_option(response.options, &option_offset, 51, 4, (uint8_t*)&lease_time);

            uint32_t server_id = inet_addr(SERVER_IP);
            add_dhcp_option(response.options, &option_offset, 54, 4, (uint8_t*)&server_id);

            response.options[option_offset++] = 255; // End option

            // Create a UDP socket
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) {
                perror("Socket creation failed");
                pthread_mutex_unlock(&lease_mutex);
                return;
            }

            // Send the DHCP ACK response
            ssize_t sent_len = sendto(sock, &response, sizeof(DHCPPacket), 0,
                                      (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
            if (sent_len < 0) {
                perror("Sendto failed");
                close(sock);
                pthread_mutex_unlock(&lease_mutex);
                return;
            }

            close(sock); // Close the socket after sending the response
            write_log("Lease renewed successfully");
            pthread_mutex_unlock(&lease_mutex); // Unlock the mutex
            return;
        }
    }
    pthread_mutex_unlock(&lease_mutex); // Unlock if no matching lease is found

    write_log("Lease renewal failed: IP not found");
}


// Implement remote network support and DHCP relay handling
void handle_remote_request(DHCPPacket *packet, struct sockaddr_in *client_addr) {
    if (packet->giaddr != 0) {
        // This is a relayed request
        write_log("Handling relayed DHCP request");
        // Set the gateway IP address in the response
        packet->giaddr = packet->giaddr;
    } else {
        write_log("Handling direct DHCP request");
    }
}



void handle_dhcp_discover(DHCPPacket *packet, struct sockaddr_in *client_addr) {
    printf("Handling DHCP Discover\n");
    char log_message[256];
    snprintf(log_message, sizeof(log_message), "Handling DHCP Discover from %s", inet_ntoa(client_addr->sin_addr));
    write_log(log_message);
    DHCPPacket response;
    memset(&response, 0, sizeof(DHCPPacket));

    response.op = 2; // Boot Reply
    response.htype = packet->htype;
    response.hlen = packet->hlen;
    response.xid = packet->xid;
    response.yiaddr = get_next_available_ip();
    response.siaddr = inet_addr(SERVER_IP);
    memcpy(response.chaddr, packet->chaddr, 16);

    int option_offset = 0;

    uint8_t dhcp_msg_type = 2; // DHCP Offer
    add_dhcp_option(response.options, &option_offset, 53, 1, &dhcp_msg_type);

    uint32_t lease_time = htonl(86400); // 24 hours
    add_dhcp_option(response.options, &option_offset, 51, 4, (uint8_t*)&lease_time);

    uint32_t server_id = inet_addr(SERVER_IP);
    add_dhcp_option(response.options, &option_offset, 54, 4, (uint8_t*)&server_id);

    // Add subnet mask option
    uint32_t subnet_mask = htonl(0xFFFFFF00); // 255.255.255.0
    add_dhcp_option(response.options, &option_offset, 1, 4, (uint8_t*)&subnet_mask);

    // Add router option (default gateway)
    uint32_t router = inet_addr(SERVER_IP); // Using server IP as gateway
    add_dhcp_option(response.options, &option_offset, 3, 4, (uint8_t*)&router);

    // Add DNS server option
    add_dhcp_option(response.options, &option_offset, 6, 4, (uint8_t*)&server_id);

    response.options[option_offset++] = 255; // End option

    response.options[option_offset++] = 255; // End option

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }



    // Set SO_REUSEADDR to allow re-binding to the same port
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sock);
        return;
    } 
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DHCP_SERVER_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("1Bind failed");
        close(sock);
        return;
    }

    client_addr->sin_port = htons(DHCP_CLIENT_PORT);
    printf("Sending DHCP Offer to client\n");

    if (sendto(sock, &response, sizeof(DHCPPacket), 0, (struct sockaddr *)client_addr, sizeof(*client_addr)) < 0) {
        perror("Sendto failed");
    }

    close(sock);
}

void handle_dhcp_request(DHCPPacket *packet, struct sockaddr_in *client_addr) {
    printf("Handling DHCP Request\n");
    
    char log_message[256];
    snprintf(log_message, sizeof(log_message), "Handling DHCP Request from %s", inet_ntoa(client_addr->sin_addr));
    write_log(log_message);

    // Prepare the DHCP response
    DHCPPacket response;
    memset(&response, 0, sizeof(DHCPPacket));

    response.op = 2; // Boot Reply
    response.htype = packet->htype;
    response.hlen = packet->hlen;
    response.xid = packet->xid;
    response.siaddr = inet_addr(SERVER_IP); // Server's IP address
    memcpy(response.chaddr, packet->chaddr, 16); // Copy client MAC address

    // Add DHCP options to the response
    int option_offset = 0;
    uint8_t dhcp_msg_type = 2; // DHCP Offer (if offering an IP)
    add_dhcp_option(response.options, &option_offset, 53, 1, &dhcp_msg_type);

    uint32_t lease_time = 86400; // 24 hours in host byte order
    uint32_t lease_time_network = htonl(lease_time); // Convert to network byte order for transmission
    add_dhcp_option(response.options, &option_offset, 51, 4, (uint8_t*)&lease_time_network);

    uint32_t server_id = inet_addr(SERVER_IP); // Server identifier
    add_dhcp_option(response.options, &option_offset, 54, 4, (uint8_t*)&server_id);

    response.options[option_offset++] = 255; // End option

    // Assign an IP address to the client and add it to the lease table
    pthread_mutex_lock(&lease_mutex);

    if (num_leases < MAX_CLIENTS) {
        // Select an IP address (you should have a pool of available addresses)
        // For simplicity, I'm using a static IP pool.
        char new_ip[16] = "192.168.1.100"; // This should ideally come from a pool

        // Set the response IP (yiaddr) to the new lease IP address
        response.yiaddr = inet_addr(new_ip);

        // Store the lease in the lease table
        snprintf(ip_leases[num_leases].ip, sizeof(ip_leases[num_leases].ip), "%s", new_ip);
        snprintf(ip_leases[num_leases].mac, sizeof(ip_leases[num_leases].mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                 packet->chaddr[0], packet->chaddr[1], packet->chaddr[2],
                 packet->chaddr[3], packet->chaddr[4], packet->chaddr[5]);
        ip_leases[num_leases].lease_time = lease_time;
        ip_leases[num_leases].lease_start = time(NULL); // Record when the lease starts

        printf("Assigned IP: %s to MAC: %s\n", ip_leases[num_leases].ip, ip_leases[num_leases].mac);
        num_leases++;
    } else {
        printf("No available lease slots\n");
        pthread_mutex_unlock(&lease_mutex);
        return;
    }

    pthread_mutex_unlock(&lease_mutex);

    // Create a UDP socket for sending the response
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    // Set the client port for response
    client_addr->sin_port = htons(DHCP_CLIENT_PORT);

    // Send the DHCP Offer (response)
    printf("Sending DHCP Offer to client\n");

    ssize_t sent_bytes = sendto(sock, &response, sizeof(DHCPPacket), 0, (struct sockaddr *)client_addr, sizeof(*client_addr));
    if (sent_bytes < 0) {
        perror("Sendto failed");
    }

    // Close the socket after sending the response
    close(sock);
}

void* dhcp_server_thread(void* arg) {
    printf("Starting DHCP server...\n");
    write_log("Starting DHCP server...");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DHCP_SERVER_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("2Bind failed");
        close(sock);
        return NULL;
    }

while (1) {
        DHCPPacket packet;
        socklen_t client_len = sizeof(client_addr);
        write_log("Waiting for DHCP packet...");

        ssize_t received = recvfrom(sock, &packet, sizeof(DHCPPacket), 0, (struct sockaddr *)&client_addr, &client_len);
        if (received < 0) {
            write_log("Recvfrom failed");
            continue;
        }

        char log_message[256];
        snprintf(log_message, sizeof(log_message), "Received DHCP packet from %s", inet_ntoa(client_addr.sin_addr));
        write_log(log_message);

        // Process DHCP packet
        uint8_t msg_type = 0;
        for (int i = 0; i < sizeof(packet.options); i++) {
            if (packet.options[i] == 53 && i + 1 < sizeof(packet.options)) { // DHCP Message Type option
                msg_type = packet.options[i + 2];
                break;
            }
        }

        snprintf(log_message, sizeof(log_message), "DHCP message type: %d", msg_type);
        write_log(log_message);

        switch (msg_type) {
            case 1: // DHCP Discover
                handle_dhcp_discover(&packet, &client_addr);
                break;
            case 3: // DHCP Request
                handle_dhcp_request(&packet, &client_addr);
                break;
            default:
                snprintf(log_message, sizeof(log_message), "Unsupported DHCP message type: %d", msg_type);
                write_log(log_message);
        }
    }

    close(sock);
    return NULL;
}
int main() {
    init_log();
    write_log("DHCP Server starting...");
    
    // Initialize DNS entries
    add_dns_entry("example.com", "93.184.216.34");
    add_dns_entry("google.com", "172.217.16.142");

    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, dhcp_server_thread, NULL) != 0) {
        write_log("Failed to create server thread");
        close_log();
        return 1;
    }

    pthread_join(server_thread, NULL);

    write_log("DHCP Server shutting down...");
    close_log();
    return 0;
}