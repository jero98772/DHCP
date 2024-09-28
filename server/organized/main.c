#include <pthread.h>
#include "dhcp_utils.h"
#include "dhcp_phases.h"

void* dhcp_server_thread(void* arg) {
    printf("Starting DHCP server...\n");
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
    server_addr.sin_port = htons(DHCP_SERVER_PORT);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
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

        // Handle the received packet
        handle_remote_request(&packet, &client_addr);
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