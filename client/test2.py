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
SERVER_PORT = 6765
CLIENT_PORT = 6866

def create_dhcp_packet(message_type, xid, requested_ip=None):
    packet = struct.pack('!BBBBIHH', 1, 1, 6, 0, xid, 0, 0)
    packet += socket.inet_aton('0.0.0.0')  # ciaddr
    packet += socket.inet_aton('0.0.0.0')  # yiaddr
    packet += socket.inet_aton('0.0.0.0')  # siaddr
    packet += socket.inet_aton('0.0.0.0')  # giaddr
    packet += b'\x00' * 16  # chaddr (client hardware address)
    packet += b'\x00' * 64  # sname
    packet += b'\x00' * 128  # file
    packet += b'\x63\x82\x53\x63'  # magic cookie

    # DHCP Message Type option
    packet += struct.pack('!BBB', DHCP_MESSAGE_TYPE, 1, message_type)

    if requested_ip:
        packet += struct.pack('!BBB4s', DHCP_REQUESTED_IP, 4, 4, socket.inet_aton(requested_ip))

    packet += b'\xff'  # End option
    return packet

def parse_dhcp_packet(packet):
    if len(packet) < 240:
        return None

    fields = struct.unpack('!BBBBIHH4s4s4s4s', packet[:28])
    options = packet[240:]

    parsed_packet = {
        'op': fields[0],
        'htype': fields[1],
        'hlen': fields[2],
        'hops': fields[3],
        'xid': fields[4],
        'secs': fields[5],
        'flags': fields[6],
        'ciaddr': socket.inet_ntoa(fields[7]),
        'yiaddr': socket.inet_ntoa(fields[8]),
        'siaddr': socket.inet_ntoa(fields[9]),
        'giaddr': socket.inet_ntoa(fields[10]),
    }

    while options:
        option_type = options[0]
        if option_type == 0xff:
            break
        option_length = options[1]
        option_value = options[2:2+option_length]
        options = options[2+option_length:]

        if option_type == DHCP_MESSAGE_TYPE:
            parsed_packet['message_type'] = option_value[0]
        elif option_type == DHCP_SERVER_ID:
            parsed_packet['server_id'] = socket.inet_ntoa(option_value)
        elif option_type == DHCP_LEASE_TIME:
            parsed_packet['lease_time'] = struct.unpack('!I', option_value)[0]

    return parsed_packet

def dhcp_client():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('', CLIENT_PORT))

    xid = random.randint(0, 0xFFFFFFFF)

    print("Sending DHCP Discover...")
    discover_packet = create_dhcp_packet(DHCP_DISCOVER, xid)
    sock.sendto(discover_packet, (SERVER_IP, SERVER_PORT))

    while True:
        print("Waiting for DHCP Offer...")
        data, addr = sock.recvfrom(1024)
        offer = parse_dhcp_packet(data)

        if offer and offer['message_type'] == DHCP_OFFER:
            print(f"Received DHCP Offer. IP: {offer['yiaddr']}")
            break

    print("Sending DHCP Request...")
    request_packet = create_dhcp_packet(DHCP_REQUEST, xid, requested_ip=offer['yiaddr'])
    sock.sendto(request_packet, (SERVER_IP, SERVER_PORT))

    while True:
        print("Waiting for DHCP ACK...")
        data, addr = sock.recvfrom(1024)
        ack = parse_dhcp_packet(data)

        if ack and ack['message_type'] == DHCP_ACK:
            print(f"Received DHCP ACK. IP: {ack['yiaddr']}, Lease Time: {ack['lease_time']} seconds")
            break

    sock.close()
    return ack['yiaddr'], ack['lease_time']

if __name__ == "__main__":
    assigned_ip, lease_time = dhcp_client()
    print(f"DHCP process completed. Assigned IP: {assigned_ip}, Lease Time: {lease_time} seconds")
