, sizeof(DHCPPacket));
            
            response.op = 2; // Boot Reply
            response.htype = packet->htype;
            response.hlen = packet->hlen;
            response.xid = packet->xid;
            response.yiaddr = inet_addr(ip_leases[i].ip);
            response.siaddr = inet_addr(SERVER_IP);
            memcpy(response.chaddr, packet->chaddr, 16);

            int option_offset = 0;
            uint8_t dhcp_msg_type = 5; // DHCP ACK
            add_dhcp_option(response.options, &option_offset, 53, 1, &dhcp_msg_type);

            uint32_t lease_time = htonl(ip_leases[i].lease_time);
            add_dhcp_option(response.options, &option_offset, 51, 4, (uint8_t*)&lease_time);

            uint32_t server_id = inet_addr(SERVER_IP);
            add_dhcp_option(response.options, &option_offset, 54, 4, (uint8_t*)&server_id);

            response.options[option_offset++] = 255; // End option

            // Send the ACK
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) {
                perror("Socket creation failed");
                pthread_mutex_unlock(&lease_mutex);
                return;
            }

            if (sendto(sock, &response, sizeof(DHCPPacket), 0, (struct sockaddr *)client_addr, sizeof(*client_addr)) < 0) {
                perror("Sendto failed");
            }

            close(sock);
            write_log("Lease renewed successfully");
            pthread_mutex_unlock(&lease_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&lease_mutex);
    
    write_log("Lease renewal failed: IP not found");
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
    
    // Determine the type of DHCP message
    uint8_t msg_type = 0;
    for (int i = 0; i < sizeof(packet->options); i++) {
        if (packet->options[i] == 53 && i + 1 < sizeof(packet->options)) { // DHCP Message Type option
            msg_type = packet->options[i + 2];
            break;
        }
    }

    // Handle the request based on its type
    switch (msg_type) {
        case 1: // DHCP Discover
            handle_dhcp_discover(packet, client_addr);
            break;
        case 3: // DHCP Request
            handle_dhcp_request(packet, client_addr);
            break;
        case 7: // DHCP Release
            // Implement DHCP Release handling if needed
            write_log("DHCP Release received - not implemented");
            break;
        case 8: // DHCP Inform
            // Implement DHCP Inform handling if needed
            write_log("DHCP Inform received - not implemented");
            break;
        default:
            write_log("Unsupported DHCP message type received");
    }
}