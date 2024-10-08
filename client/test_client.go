package main

import (
	"encoding/binary"
	"fmt"
	"net"
	"time"
)

const (
	DHCPServerPort    = 6767
	DHCPClientPort    = 6868
	MaxDHCPPacketSize = 1024
)

type DHCPPacket struct {
	Op      byte
	Htype   byte
	Hlen    byte
	Hops    byte
	Xid     uint32
	Secs    uint16
	Flags   uint16
	Ciaddr  [4]byte
	Yiaddr  [4]byte
	Siaddr  [4]byte
	Giaddr  [4]byte
	Chaddr  [16]byte
	Sname   [64]byte
	File    [128]byte
	Options []byte
}

func (p *DHCPPacket) Marshal() []byte {
	fmt.Println("Marshaling DHCP packet...")
	buf := make([]byte, 240+len(p.Options))
	buf[0] = p.Op
	buf[1] = p.Htype
	buf[2] = p.Hlen
	buf[3] = p.Hops
	binary.BigEndian.PutUint32(buf[4:8], p.Xid)
	binary.BigEndian.PutUint16(buf[8:10], p.Secs)
	binary.BigEndian.PutUint16(buf[10:12], p.Flags)
	copy(buf[12:16], p.Ciaddr[:])
	copy(buf[16:20], p.Yiaddr[:])
	copy(buf[20:24], p.Siaddr[:])
	copy(buf[24:28], p.Giaddr[:])
	copy(buf[28:44], p.Chaddr[:])
	copy(buf[44:108], p.Sname[:])
	copy(buf[108:236], p.File[:])
	copy(buf[236:], p.Options)
	return buf
}

func (p *DHCPPacket) Unmarshal(data []byte) error {
	fmt.Println("Unmarshaling DHCP packet...")
	if len(data) < 240 {
		return fmt.Errorf("packet too short")
	}
	p.Op = data[0]
	p.Htype = data[1]
	p.Hlen = data[2]
	p.Hops = data[3]
	p.Xid = binary.BigEndian.Uint32(data[4:8])
	p.Secs = binary.BigEndian.Uint16(data[8:10])
	p.Flags = binary.BigEndian.Uint16(data[10:12])
	copy(p.Ciaddr[:], data[12:16])
	copy(p.Yiaddr[:], data[16:20])
	copy(p.Siaddr[:], data[20:24])
	copy(p.Giaddr[:], data[24:28])
	copy(p.Chaddr[:], data[28:44])
	copy(p.Sname[:], data[44:108])
	copy(p.File[:], data[108:236])
	p.Options = data[236:]
	return nil
}

func addDHCPOption(options []byte, optionType byte, optionValue []byte) []byte {
	fmt.Printf("Adding DHCP option: %d\n", optionType)
	return append(options, optionType, byte(len(optionValue)))
}

func createDHCPDiscover(macAddr net.HardwareAddr) *DHCPPacket {
    fmt.Printf("Creating DHCP Discover packet with MAC address: %v\n", macAddr)
    packet := &DHCPPacket{
        Op:    1,
        Htype: 1,
        Hlen:  6,
        Xid:   uint32(time.Now().UnixNano()),
        Flags: 0x8000, // Broadcast
    }
    copy(packet.Chaddr[:], macAddr)

    // Add DHCP options
    packet.Options = []byte{
        53, 1, 1,  // DHCP Discover (option 53, length 1, type 1)
        55, 4, 1, 3, 6, 15,  // Parameter Request List
        255,  // End option
    }

    return packet
}

func createDHCPRequest(offer *DHCPPacket, macAddr net.HardwareAddr) *DHCPPacket {
	fmt.Printf("Creating DHCP Request packet with XID: %d\n", offer.Xid)
	packet := &DHCPPacket{
		Op:    1,
		Htype: 1,
		Hlen:  6,
		Xid:   offer.Xid,
		Flags: 0x8000, // Broadcast
	}
	copy(packet.Chaddr[:], macAddr)
	copy(packet.Yiaddr[:], offer.Yiaddr[:])

	// Add DHCP options
	packet.Options = addDHCPOption(packet.Options, 53, []byte{3})          // DHCP Request
	packet.Options = addDHCPOption(packet.Options, 50, offer.Yiaddr[:])    // Requested IP Address
	packet.Options = addDHCPOption(packet.Options, 54, offer.Siaddr[:])    // DHCP Server Identifier
	packet.Options = append(packet.Options, 255)                           // End option

	return packet
}

func dhcpClient() error {
	fmt.Println("Starting DHCP client...")
	// Create UDP connection
	conn, err := net.ListenUDP("udp4", &net.UDPAddr{Port: DHCPClientPort})
	if err != nil {
		return fmt.Errorf("failed to create UDP connection: %v", err)
	}
	defer conn.Close()

	fmt.Println("UDP connection created on client port:", DHCPClientPort)

	// Get interface MAC address
	ifaces, err := net.Interfaces()
	if err != nil {
		return fmt.Errorf("failed to get network interfaces: %v", err)
	}
	var macAddr net.HardwareAddr
	for _, iface := range ifaces {
		if iface.Flags&net.FlagLoopback == 0 && iface.Flags&net.FlagUp != 0 {
			macAddr = iface.HardwareAddr
			fmt.Printf("Selected interface: %s with MAC: %v\n", iface.Name, macAddr)
			break
		}
	}
	if macAddr == nil {
		return fmt.Errorf("failed to find a suitable network interface")
	}

	// Send DHCP Discover
	fmt.Println("Sending DHCP Discover...")
	discoverPacket := createDHCPDiscover(macAddr)
	_, err = conn.WriteToUDP(discoverPacket.Marshal(), &net.UDPAddr{IP: net.IPv4bcast, Port: DHCPServerPort})
	if err != nil {
		return fmt.Errorf("failed to send DHCP Discover: %v", err)
	}

	// Receive DHCP Offer
	fmt.Println("Waiting for DHCP Offer...")
	offerBuf := make([]byte, MaxDHCPPacketSize)
	n, _, err := conn.ReadFromUDP(offerBuf)
	if err != nil {
		return fmt.Errorf("failed to receive DHCP Offer: %v", err)
	}
	offerPacket := &DHCPPacket{}
	err = offerPacket.Unmarshal(offerBuf[:n])
	if err != nil {
		return fmt.Errorf("failed to unmarshal DHCP Offer: %v", err)
	}
	fmt.Printf("Received DHCP Offer with XID: %d\n", offerPacket.Xid)

	// Send DHCP Request
	fmt.Println("Sending DHCP Request...")
	requestPacket := createDHCPRequest(offerPacket, macAddr)
	_, err = conn.WriteToUDP(requestPacket.Marshal(), &net.UDPAddr{IP: net.IPv4bcast, Port: DHCPServerPort})
	if err != nil {
		return fmt.Errorf("failed to send DHCP Request: %v", err)
	}

	// Receive DHCP ACK
	fmt.Println("Waiting for DHCP ACK...")
	ackBuf := make([]byte, MaxDHCPPacketSize)
	n, _, err = conn.ReadFromUDP(ackBuf)
	if err != nil {
		return fmt.Errorf("failed to receive DHCP ACK: %v", err)
	}
	ackPacket := &DHCPPacket{}
	err = ackPacket.Unmarshal(ackBuf[:n])
	if err != nil {
		return fmt.Errorf("failed to unmarshal DHCP ACK: %v", err)
	}

	fmt.Printf("Received IP address: %v\n", net.IP(ackPacket.Yiaddr[:]))
	return nil
}

func main() {
    fmt.Println("Starting DHCP client...")

    // Create a UDP connection
    serverAddr, err := net.ResolveUDPAddr("udp", fmt.Sprintf("255.255.255.255:%d", DHCPServerPort))
    if err != nil {
        fmt.Println("Error resolving server address:", err)
        return
    }

    clientAddr, err := net.ResolveUDPAddr("udp", fmt.Sprintf("127.0.0.1:%d", DHCPClientPort))
    if err != nil {
        fmt.Println("Error resolving client address:", err)
        return
    }

    conn, err := net.ListenUDP("udp", clientAddr)
    if err != nil {
        fmt.Println("Error creating UDP connection:", err)
        return
    }
    defer conn.Close()

    // Get interface MAC address
    ifaces, err := net.Interfaces()
    if err != nil {
        fmt.Println("Error getting network interfaces:", err)
        return
    }
    var macAddr net.HardwareAddr
    for _, iface := range ifaces {
        if iface.Flags&net.FlagLoopback == 0 && iface.Flags&net.FlagUp != 0 {
            macAddr = iface.HardwareAddr
            fmt.Printf("Selected interface: %s with MAC: %v\n", iface.Name, macAddr)
            break
        }
    }
    if macAddr == nil {
        fmt.Println("Failed to find a suitable network interface")
        return
    }

    // Create and send DHCP Discover packet
    discoverPacket := createDHCPDiscover(macAddr)
    packetData := discoverPacket.Marshal()
    _, err = conn.WriteToUDP(packetData, serverAddr)
    if err != nil {
        fmt.Println("Error sending DHCP Discover:", err)
        return
    }

    // Wait for DHCP Offer
    fmt.Println("Waiting for DHCP Offer...")
    buffer := make([]byte, MaxDHCPPacketSize)
    n, _, err := conn.ReadFromUDP(buffer)
    if err != nil {
        fmt.Println("Error receiving DHCP Offer:", err)
        return
    }
    fmt.Printf("Received DHCP Offer, %d bytes\n", n)

    // Process DHCP Offer here...
    // (You would typically send a DHCP Request and wait for an ACK)
}