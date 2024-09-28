#ifndef DHCP_UTILS_H
#define DHCP_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 100
#define IP_POOL_START "192.168.1.100"
#define IP_POOL_END "192.168.1.200"
#define SERVER_IP "127.0.0.1"
#define DHCP_SERVER_PORT 6767
#define DHCP_CLIENT_PORT 6868
#define MAX_DHCP_PACKET_SIZE 1024
#define LOG_FILE "dhcp_server.log"

extern FILE *log_file;
extern pthread_mutex_t log_mutex;
extern pthread_mutex_t lease_mutex;

typedef struct {
    char ip[16];
    char mac[18];
    time_t lease_start;
    int lease_time;
    int state; // 0: free, 1: offered, 2: leased
} IPLease;

extern IPLease ip_leases[MAX_CLIENTS];
extern int num_leases;

void init_log();
void close_log();
void write_log(const char *message);
uint32_t get_next_available_ip();
void add_dhcp_option(uint8_t *options, int *offset, uint8_t option_code, uint8_t option_length, uint8_t *option_value);

// DNS related functions
void add_dns_entry(const char* domain, const char* ip);
const char* lookup_dns(const char* domain);

#endif // DHCP_UTILS_H