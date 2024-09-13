# DHCP

### Ideas

dns sever as a hashmap


### comments of the teacher

subneting not mean something strange, just can be accsesible from outside and hosted in aws

el cliente debe mandar peticiones de forma repetitiva

comnication in brodcast send to all

with 2 ports

1 for send from the client to the server

2 for send from the server to the client

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

### run client in python

	python client.py


### references


https://github.com/ejt0062/dhcpserver-c/tree/master

https://github.com/crossbowerbt/dhcpserver