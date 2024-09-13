package main

import (
	"encoding/binary"
	"fmt"
	"net"
	"time"
)

const (
	DHCPServerPort = 67
	DHCPClientPort = 68
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
	copy(p.Yi