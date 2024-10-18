# DHCP

### Video

https://youtu.be/zZ7pqyRhiCs

### Ideas

dns sever as a hashmap


### comments of the teacher

subneting not mean something strange, just can be accsesible from outside and hosted in aws

el cliente debe mandar peticiones de forma repetitiva

comnication in brodcast send to all

with 2 ports

1 for send from the client to the server port 67/udp

2 for send from the server to the client port 68/udp

dhcp discover from start, it send all users

we can use a dns hashmap

time of leassing , is time of that device will have that address

log for time lessing

offer is in brodcast for all 

request for the client, asking for the parameters


67/UDP (servidor)
68/UDP (cliente)

### run server in c

	gcc server.c -o server
	./server

new server

	gcc -o dhcp_server test_server.c -lpthread

	sudo ./dhcp_server

### run client in python

	python client.py

	sudo go run test_client.go 


### references


https://github.com/ejt0062/dhcpserver-c/tree/master

https://github.com/crossbowerbt/dhcpserver

https://github.com/playma/simple_dhcp

### todo 

add log file

change ports
