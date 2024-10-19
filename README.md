# DHCP

### Introduccion

Para poder entender nuestro proyecto primero debemos entender que es un DHCP, un DHCP es un protocolo de red utilizado para asignar automáticamente direcciones IP y otros parámetros de configuración a dispositivos en una red. Su principal objetivo es simplificar la administración de direcciones IP y garantizar que cada dispositivo tenga una dirección IP única, sin la necesidad de configurarlas manualmente. Este sistema es ampliamente utilizado en redes locales (LAN), y es esencial para grandes infraestructuras como campus universitarios, oficinas, o incluso redes domésticas con múltiples dispositivos.

En nuestro caso el servidor es capaz de responder a las solicitudes DHCP DISCOVER con una oferta de dirección IP, confirmar la asignación de direcciones IP mediante DHCP ACK, y liberar direcciones IP una vez expiradas o declinadas. También incluye un sistema básico de resolución de nombres DNS.

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
