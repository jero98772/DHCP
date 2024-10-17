#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define MAX_CLIENTS 100
#define IP_POOL_START "192.168.1.100"
#define IP_POOL_END "192.168.1.200"
#define SERVER_IP "0.0.0.0"
#define DHCP_SERVER_PORT 667
#define DHCP_CLIENT_PORT 668
#define MAX_DHCP_PACKET_SIZE 1024
#define LOG_FILE "dhcp_server.log"
#define CONFIG_FILE "dhcp_config.txt"
#define MAX_DNS_ENTRIES 100
#define MAX_DOMAIN_NAME_LENGTH 256
#define LEASE_THRESHOLD 0.8

FILE *log_file = NULL;
int server_running = 1;

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

typedef struct {
    char domain[MAX_DOMAIN_NAME_LENGTH];
    char ip[16];
} DNSEntry;

IPLease ip_leases[MAX_CLIENTS];
int num_leases = 0;

DNSEntry dns_table[MAX_DNS_ENTRIES];
int dns_entries = 0;

char ip_pool_start[16];
char ip_pool_end[16];
char server_ip[16];
int dhcp_server_port;
int dhcp_client_port;
int default_lease_time;

void load_config() {
    FILE *config_file = fopen(CONFIG_FILE, "r");
    if (config_file == NULL) {
        fprintf(stderr, "Error opening config file. Using default values.\n");
        strcpy(ip_pool_start, IP_POOL_START);
        strcpy(ip_pool_end, IP_POOL_END);
        strcpy(server_ip, SERVER_IP);
        dhcp_server_port = DHCP_SERVER_PORT;
        dhcp_client_port = DHCP_CLIENT_PORT;
        default_lease_time = 86400; // 24 hours
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), config_file)) {
        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            if (strcmp(key, "ip_pool_start") == 0) strcpy(ip_pool_start, value);
            else if (strcmp(key, "ip_pool_end") == 0) strcpy(ip_pool_end, value);
            else if (strcmp(key, "server_ip") == 0) strcpy(server_ip, value);
            else if (strcmp(key, "dhcp_server_port") == 0) dhcp_server_port = atoi(value);
            else if (strcmp(key, "dhcp_client_port") == 0) dhcp_client_port = atoi(value);
            else if (strcmp(key, "default_lease_time") == 0) default_lease_time = atoi(value);
        }
    }

    fclose(config_file);
}
int create_and_bind_socket(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Allow reuse of local addresses
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sock);
        return -1;
    }

    // Allow reuse of ports (BSD-specific, might not be necessary on all systems)
    #ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
        // Not critical, so we'll continue even if this fails
    }
    #endif

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Bind failed: %s", strerror(errno));
        perror(error_msg);
        close(sock);
        return -1;
    }

    return sock;
}
void add_dns_entry(const char* domain, const char* ip) {
    if (dns_entries < MAX_DNS_ENTRIES) {
        strncpy(dns_table[dns_entries].domain, domain, MAX_DOMAIN_NAME_LENGTH - 1);
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
    uint32_t start_ip = inet_addr(ip_pool_start);
    uint32_t end_ip = inet_addr(ip_pool_end);

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

void add_dhcp_option(uint8_t *options, int *offset, uint8_t option_code, uint8_t option_length, uint8_t *option_value) {
    options[(*offset)++] = option_code;
    options[(*offset)++] = option_length;
    memcpy(&options[*offset], option_value, option_length);
    *offset += option_length;
}

void handle_dhcp_discover(DHCPPacket *packet, struct sockaddr_in *client_addr) {
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
    response.siaddr = inet_addr(server_ip);
    memcpy(response.chaddr, packet->chaddr, 16);

    int option_offset = 0;
    uint8_t dhcp_msg_type = 2; // DHCP Offer
    add_dhcp_option(response.options, &option_offset, 53, 1, &dhcp_msg_type);

    uint32_t lease_time = htonl(default_lease_time);
    add_dhcp_option(response.options, &option_offset, 51, 4, (uint8_t*)&lease_time);

    uint32_t server_id = inet_addr(server_ip);
    add_dhcp_option(response.options, &option_offset, 54, 4, (uint8_t*)&server_id);

    // Add subnet mask option
    uint32_t subnet_mask = htonl(0xFFFFFF00); // 255.255.255.0
    add_dhcp_option(response.options, &option_offset, 1, 4, (uint8_t*)&subnet_mask);

    // Add router option (default gateway)
    uint32_t router = inet_addr(server_ip); // Using server IP as gateway
    add_dhcp_option(response.options, &option_offset, 3, 4, (uint8_t*)&router);

    // Add DNS server option
    add_dhcp_option(response.options, &option_offset, 6, 4, (uint8_t*)&server_id);

    response.options[option_offset++] = 255; // End option

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(dhcp_server_port);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Bind failed: %s", strerror(errno));
        write_log(error_msg);
        perror("Bind failed");
        close(sock);
        return ;
    }

    client_addr->sin_port = htons(dhcp_client_port);

    if (sendto(sock, &response, sizeof(DHCPPacket), 0, (struct sockaddr *)client_addr, sizeof(*client_addr)) < 0) {
        perror("Sendto failed");
    } else {
        write_log("Sent DHCP Offer to client");
    }

    close(sock);
}

void handle_dhcp_request(DHCPPacket *packet, struct sockaddr_in *client_addr) {
    char log_message[256];
    snprintf(log_message, sizeof(log_message), "Handling DHCP Request from %s", inet_ntoa(client_addr->sin_addr));
    write_log(log_message);

    DHCPPacket response;
    memset(&response, 0, sizeof(DHCPPacket));

    response.op = 2; // Boot Reply
    response.htype = packet->htype;
    response.hlen = packet->hlen;
    response.xid = packet->xid;
    response.yiaddr = packet->yiaddr;
    response.siaddr = inet_addr(server_ip);
    memcpy(response.chaddr, packet->chaddr, 16);

    int option_offset = 0;
    uint8_t dhcp_msg_type = 5; // DHCP ACK
    add_dhcp_option(response.options, &option_offset, 53, 1, &dhcp_msg_type);

    uint32_t lease_time = htonl(default_lease_time);
    add_dhcp_option(response.options, &option_offset, 51, 4, (uint8_t*)&lease_time);

    uint32_t server_id = inet_addr(server_ip);
    add_dhcp_option(response.options, &option_offset, 54, 4, (uint8_t*)&server_id);

    // Add subnet mask option
    uint32_t subnet_mask = htonl(0xFFFFFF00); // 255.255.255.0
    add_dhcp_option(response.options, &option_offset, 1, 4, (uint8_t*)&subnet_mask);

    // Add router option (default gateway)
    uint32_t router = inet_addr(server_ip); // Using server IP as gateway
    add_dhcp_option(response.options, &option_offset, 3, 4, (uint8_t*)&router);

    // Add DNS server option
    add_dhcp_option(response.options, &option_offset, 6, 4, (uint8_t*)&server_id);

    response.options[option_offset++] = 255; // End option

    // Add lease to the list
    pthread_mutex_lock(&lease_mutex);
    if (num_leases < MAX_CLIENTS) {
        struct in_addr addr;
        addr.s_addr = packet->yiaddr;
        inet_ntop(AF_INET, &addr, ip_leases[num_leases].ip, sizeof(ip_leases[num_leases].ip));
        snprintf(ip_leases[num_leases].mac, sizeof(ip_leases[num_leases].mac), "%02x:%02x:%02x:%02x:%02x:%02x",
                 packet->chaddr[0], packet->chaddr[1], packet->chaddr[2],
                 packet->chaddr[3], packet->chaddr[4], packet->chaddr[5]);
        ip_leases[num_leases].lease_start = time(NULL);
        ip_leases[num_leases].lease_time = ntohl(lease_time);
        ip_leases[num_leases].state = 2; // Leased
        num_leases++;
        snprintf(log_message, sizeof(log_message), "Assigned IP: %s to MAC: %s", ip_leases[num_leases-1].ip, ip_leases[num_leases-1].mac);
        write_log(log_message);
    }
    else {
        write_log("No available lease slots");
    }
    pthread_mutex_unlock(&lease_mutex);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(dhcp_server_port);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Bind failed: %s", strerror(errno));
        write_log(error_msg);
        perror("Bind failed");
        close(sock);
        return ;
    }

client_addr->sin_port = htons(dhcp_client_port);

    if (sendto(sock, &response, sizeof(DHCPPacket), 0, (struct sockaddr *)client_addr, sizeof(*client_addr)) < 0) {
        perror("Sendto failed");
    } else {
        write_log("Sent DHCP ACK to client");
    }

    close(sock);
}

void handle_dhcp_release(DHCPPacket *packet) {
    char client_ip[16];
    inet_ntop(AF_INET, &(packet->ciaddr), client_ip, sizeof(client_ip));
    
    pthread_mutex_lock(&lease_mutex);
    for (int i = 0; i < num_leases; i++) {
        if (strcmp(ip_leases[i].ip, client_ip) == 0) {
            ip_leases[i].state = 0; // Free
            char log_message[256];
            snprintf(log_message, sizeof(log_message), "Released IP: %s", client_ip);
            write_log(log_message);
            break;
        }
    }
    pthread_mutex_unlock(&lease_mutex);
}

void handle_dhcp_decline(DHCPPacket *packet) {
    char client_ip[16];
    inet_ntop(AF_INET, &(packet->ciaddr), client_ip, sizeof(client_ip));
    
    pthread_mutex_lock(&lease_mutex);
    for (int i = 0; i < num_leases; i++) {
        if (strcmp(ip_leases[i].ip, client_ip) == 0) {
            ip_leases[i].state = 0; // Free
            char log_message[256];
            snprintf(log_message, sizeof(log_message), "Declined IP: %s", client_ip);
            write_log(log_message);
            break;
        }
    }
    pthread_mutex_unlock(&lease_mutex);
}

void handle_dhcp_inform(DHCPPacket *packet, struct sockaddr_in *client_addr) {
    DHCPPacket response;
    memset(&response, 0, sizeof(DHCPPacket));

    response.op = 2; // Boot Reply
    response.htype = packet->htype;
    response.hlen = packet->hlen;
    response.xid = packet->xid;
    response.ciaddr = packet->ciaddr;
    response.siaddr = inet_addr(server_ip);
    memcpy(response.chaddr, packet->chaddr, 16);

    int option_offset = 0;
    uint8_t dhcp_msg_type = 5; // DHCP ACK
    add_dhcp_option(response.options, &option_offset, 53, 1, &dhcp_msg_type);

    uint32_t server_id = inet_addr(server_ip);
    add_dhcp_option(response.options, &option_offset, 54, 4, (uint8_t*)&server_id);

    // Add subnet mask option
    uint32_t subnet_mask = htonl(0xFFFFFF00); // 255.255.255.0
    add_dhcp_option(response.options, &option_offset, 1, 4, (uint8_t*)&subnet_mask);

    // Add router option (default gateway)
    uint32_t router = inet_addr(server_ip); // Using server IP as gateway
    add_dhcp_option(response.options, &option_offset, 3, 4, (uint8_t*)&router);

    // Add DNS server option
    add_dhcp_option(response.options, &option_offset, 6, 4, (uint8_t*)&server_id);

    response.options[option_offset++] = 255; // End option

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return;
    }

    if (sendto(sock, &response, sizeof(DHCPPacket), 0, (struct sockaddr *)client_addr, sizeof(*client_addr)) < 0) {
        perror("Sendto failed");
    } else {
        write_log("Sent DHCP ACK (Inform) to client");
    }

    close(sock);
}

void cleanup_expired_leases() {
    time_t current_time = time(NULL);
    pthread_mutex_lock(&lease_mutex);
    for (int i = 0; i < num_leases; i++) {
        if (ip_leases[i].state == 2 && (current_time - ip_leases[i].lease_start) > ip_leases[i].lease_time) {
            ip_leases[i].state = 0; // Free
            char log_message[256];
            snprintf(log_message, sizeof(log_message), "Expired lease for IP: %s", ip_leases[i].ip);
            write_log(log_message);
        }
    }
    pthread_mutex_unlock(&lease_mutex);
}

void* lease_cleanup_thread(void* arg) {
    while (server_running) {
        cleanup_expired_leases();
        sleep(60); // Check every minute
    }
    return NULL;
}

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

void print_dhcp_stats() {
    int total_leases = 0;
    int active_leases = 0;
    pthread_mutex_lock(&lease_mutex);
    for (int i = 0; i < num_leases; i++) {
        if (ip_leases[i].state != 0) {
            total_leases++;
            if (ip_leases[i].state == 2) {
                active_leases++;
            }
        }
    }
    pthread_mutex_unlock(&lease_mutex);

    printf("DHCP Server Statistics:\n");
    printf("Total leases: %d\n", total_leases);
    printf("Active leases: %d\n", active_leases);
    printf("Available leases: %d\n", MAX_CLIENTS - total_leases);

    double lease_usage = (double)total_leases / MAX_CLIENTS;
    if (lease_usage > LEASE_THRESHOLD) {
        printf("Warning: Lease usage is high (%.2f%%)\n", lease_usage * 100);
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        printf("Received signal %d. Shutting down...\n", signum);
        server_running = 0;
    }
}

void* dhcp_server_thread(void* arg) {
    write_log("Starting DHCP server...");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(dhcp_server_port);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Bind failed: %s", strerror(errno));
        write_log(error_msg);
        perror("Bind failed");
        close(sock);
        return NULL;
    }

    while (server_running) {
        DHCPPacket packet;
        socklen_t client_len = sizeof(client_addr);
        write_log("Waiting for DHCP packet...");

        ssize_t received = recvfrom(sock, &packet, sizeof(DHCPPacket), 0, (struct sockaddr *)&client_addr, &client_len);
        if (received < 0) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, continue loop
            }
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
            case 7: // DHCP Release
                handle_dhcp_release(&packet);
                break;
            case 4: // DHCP Decline
                handle_dhcp_decline(&packet);
                break;
            case 8: // DHCP Inform
                handle_dhcp_inform(&packet, &client_addr);
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
    
    load_config();
    
    // Initialize DNS entries
    add_dns_entry("example.com", "93.184.216.34");
    add_dns_entry("google.com", "172.217.16.142");

    //signal(SIGINT, signal_handler);
    //signal(SIGTERM, signal_handler);

    pthread_t server_thread, cleanup_thread;
    if (pthread_create(&server_thread, NULL, dhcp_server_thread, NULL) != 0) {
        write_log("Failed to create server thread");
        close_log();
        return 1;
    }

    if (pthread_create(&cleanup_thread, NULL, lease_cleanup_thread, NULL) != 0) {
        write_log("Failed to create cleanup thread");
        close_log();
        return 1;
    }

    while (server_running) {
        sleep(60); // Sleep for a minute
        print_dhcp_stats();
    }

    pthread_join(server_thread, NULL);
    pthread_join(cleanup_thread, NULL);

    write_log("DHCP Server shutting down...");
    close_log();
    return 0;
}