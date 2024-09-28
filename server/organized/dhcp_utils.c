#include "dhcp_utils.h"

FILE *log_file = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lease_mutex = PTHREAD_MUTEX_INITIALIZER;
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