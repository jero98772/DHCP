package main

import (
	"encoding/binary"
	"fmt"
	"net"
	"time"
)

const (
	DHCPServerPort    = 67
	DHCPClientPort    = 68
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
	return append(options, optionType, byte(len(optionValue)))
}

func createDHCPDiscover(macAddr net.HardwareAddr) *DHCPPacket {
	packet := &DHCPPacket{
		Op:    1,
		Htype: 1,
		Hlen:  6,
		Xid:   uint32(time.Now().UnixNano()),
		Flags: 0x8000, // Broadcast
	}
	copy(packet.Chaddr[:], macAddr)

	// Add DHCP options
	packet.Options = addDHCPOption(packet.Options, 53, []byte{1}) // DHCP Discover
	packet.Options = addDHCPOption(packet.Options, 55, []byte{1, 3, 6, 15}) // Parameter Request List
	packet.Options = append(packet.Options, 255) // End option

	return packet
}

func createDHCPRequest(offer *DHCPPacket, macAddr net.HardwareAddr) *DHCPPacket {
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
	packet.Options = addDHCPOption(packet.Options, 53, []byte{3}) // DHCP Request
	packet.Options = addDHCPOption(packet.Options, 50, offer.Yiaddr[:]) // Requested IP Address
	packet.Options = addDHCPOption(packet.Options, 54, offer.Siaddr[:]) // DHCP Server Identifier
	packet.Options = append(packet.Options, 255) // End option

	return packet
}

func dhcpClient() error {
	// Create UDP connection
	conn, err := net.ListenUDP("udp4", &net.UDPAddr{Port: DHCPClientPort})
	if err != nil {
		return fmt.Errorf("failed to create UDP connection: %v", err)
	}
	defer conn.Close()

	// Get interface MAC address
	ifaces, err := net.Interfaces()
	if err != nil {
		return fmt.Errorf("failed to get network interfaces: %v", err)
	}
	var macAddr net.HardwareAddr
	for _, iface := range ifaces {
		if iface.Flags&net.FlagLoopback == 0 && iface.Flags&net.FlagUp != 0 {
			macAddr = iface.HardwareAddr
			break
		}
	}
	if macAddr == nil {
		return fmt.Errorf("failed to find a suitable network interface")
	}

	// Send DHCP Discover
	discoverPacket := createDHCPDiscover(macAddr)
	_, err = conn.WriteToUDP(discoverPacket.Marshal(), &net.UDPAddr{IP: net.IPv4bcast, Port: DHCPServerPort})
	if err != nil {
		return fmt.Errorf("failed to send DHCP Discover: %v", err)
	}

	// Receive DHCP Offer
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

	// Send DHCP Request
	requestPacket := createDHCPRequest(offerPacket, macAddr)
	_, err = conn.WriteToUDP(requestPacket.Marshal(), &net.UDPAddr{IP: net.IPv4bcast, Port: DHCPServerPort})
	if err != nil {
		return fmt.Errorf("failed to send DHCP Request: %v", err)
	}

	// Receive DHCP ACK
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
	err := dhcpClient()
	if err != nil {
		fmt.Printf("DHCP client error: %v\n", err)
	}
}