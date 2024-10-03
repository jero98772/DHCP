import socket
import struct
import random
import time

# DHCP Message Type
DHCP_DISCOVER = 1
DHCP_OFFER = 2
DHCP_REQUEST = 3
DHCP_ACK = 5

# DHCP Options
DHCP_MESSAGE_TYPE = 53
DHCP_SERVER_ID = 54
DHCP_REQUESTED_IP = 50
DHCP_LEASE_TIME = 51

SERVER_IP = '127.0.0.1'
SERVER_PORT = 6767
CLIENT_PORT = 6868

def create_dhcp_packet(message_type, xid, requested_ip=None):
    # DHCP packet structure
    packet = struct.pack('!BBBBIHH', 
        1,      # OP (1 = BOOTREQUEST)
        1,      # HTYPE (1 = Ethernet)
        6,      # HLEN (Length of MAC address)
        0,      # HOPS
        xid,    # XID (Transaction ID)
        0,      # SECS
        0       # FLAGS
    )
    
    packet += socket.inet_aton('0.0.0.0')  # CIADDR (Client IP address)
    packet += socket.inet_aton('0.0.0.0')  # YIADDR (Your IP address)
    packet += socket.inet_aton('0.0.0.0')  # SIADDR (Server IP address)
    packet += socket.inet_aton('0.0.0.0')  # GIADDR (Gateway IP address)
    
    # CHADDR (Client hardware address, 16 bytes)
    packet += b'\x00\x0c\x29\xdd\x1d\x52' + b'\x00' * 10
    
    # SNAME (Server name, 64 bytes) and FILE (Boot filename, 128 bytes)
    packet += b'\x00' * 192
    
    # Magic cookie
    packet += b'\x63\x82\x53\x63'
    
    # DHCP Message Type option
    packet += struct.pack('!BBB', DHCP_MESSAGE_TYPE, 1, message_type)
    
    if requested_ip:
        packet += struct.pack('!BB4s', DHCP_REQUESTED_IP, 4, socket.inet_aton(requested_ip))
    
    # End option
    packet += b'\xff'
    
    return packet

def parse_dhcp_packet(data):
    if len(data) < 240:
        return None
    
    packet = {}
    packet['op'], packet['htype'], packet['hlen'], packet['hops'], \
    packet['xid'], packet['secs'], packet['flags'] = struct.unpack('!BBBBIHH', data[:12])
    
    packet['ciaddr'] = socket.inet_ntoa(data[12:16])
    packet['yiaddr'] = socket.inet_ntoa(data[16:20])
    packet['siaddr'] = socket.inet_ntoa(data[20:24])
    packet['giaddr'] = socket.inet_ntoa(data[24:28])
    packet['chaddr'] = data[28:44]
    
    if data[236:240] != b'\x63\x82\x53\x63':
        return None
    
    options = data[240:]
    while options:
        option_type = options[0]
        if option_type == 0:  # Padding
            options = options[1:]
            continue
        if option_type == 255:  # End
            break
        option_length = options[1]
        option_value = options[2:2+option_length]
        
        if option_type == DHCP_MESSAGE_TYPE:
            packet['message_type'] = option_value[0]
        elif option_type == DHCP_SERVER_ID:
            packet['server_id'] = socket.inet_ntoa(option_value)
        elif option_type == DHCP_LEASE_TIME:
            packet['lease_time'] = struct.unpack('!I', option_value)[0]
        
        options = options[2+option_length:]
    
    return packet

def dhcp_process():
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    client_socket.bind(('', CLIENT_PORT))

    xid = random.randint(0, 0xFFFFFFFF)

    print("Sending DHCP Discover...")
    discover_packet = create_dhcp_packet(DHCP_DISCOVER, xid)
    client_socket.sendto(discover_packet, (SERVER_IP, SERVER_PORT))

    print("Waiting for DHCP Offer...")
    offer_data, _ = client_socket.recvfrom(1024)
    offer_packet = parse_dhcp_packet(offer_data)
    
    if not offer_packet or offer_packet['message_type'] != DHCP_OFFER:
        print("Did not receive DHCP Offer.")
        return

    offered_ip = offer_packet['yiaddr']
    server_id = offer_packet['server_id']
    print(f"Received DHCP Offer. Offered IP: {offered_ip}")

    print("Sending DHCP Request...")
    request_packet = create_dhcp_packet(DHCP_REQUEST, xid, offered_ip)
    client_socket.sendto(request_packet, (SERVER_IP, SERVER_PORT))

    print("Waiting for DHCP ACK...")
    ack_data, _ = client_socket.recvfrom(1024)
    ack_packet = parse_dhcp_packet(ack_data)
    
    if not ack_packet or ack_packet['message_type'] != DHCP_ACK:
        print("Did not receive DHCP ACK.")
        return

    assigned_ip = ack_packet['yiaddr']
    lease_time = ack_packet.get('lease_time', 'Unknown')
    print(f"Received DHCP ACK. Assigned IP: {assigned_ip}, Lease Time: {lease_time} seconds")

    client_socket.close()

if __name__ == "__main__":
    dhcp_process()