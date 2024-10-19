# DHCP

### Introduccion

Para poder entender nuestro proyecto primero debemos entender que es un DHCP, un DHCP es un protocolo de red utilizado para asignar automáticamente direcciones IP y otros parámetros de configuración a dispositivos en una red. Su principal objetivo es simplificar la administración de direcciones IP y garantizar que cada dispositivo tenga una dirección IP única, sin la necesidad de configurarlas manualmente. Este sistema es ampliamente utilizado en redes locales (LAN), y es esencial para grandes infraestructuras como campus universitarios, oficinas, o incluso redes domésticas con múltiples dispositivos.

En nuestro caso se confirman las asignación de direcciones IP mediante DHCP ACK, y se liberan las direcciones IP una vez expiradas o declinadas. También incluye un sistema básico de resolución de nombres DNS.

### Desarrollo y Retos

Durante el desarrollo de este proyecto, nos encontramos con algunos desafíos, como era de esperarse, aunque todos fueron solucionables. El primer reto surgió al realizar pruebas de concurrencia para evaluar el comportamiento del programa bajo estas condiciones. Durante este proceso, observamos que el registro (log) mostraba información superpuesta de los usuarios, es decir, los datos se presentaban en el orden en que llegaban, sin una adecuada organización. Para resolver este inconveniente, implementamos Mutex, lo que nos permitió estructurar el log de manera ordenada y comprensible.
El segundo desafío que enfrentamos estaba relacionado con la gestión de las direcciones (addresses) utilizadas por el programa. Cada vez que el sistema asignaba una dirección a un proceso o conexión, esta permanecía abierta incluso después de finalizar su uso, lo que eventualmente podía saturar los recursos del sistema y afectar el rendimiento del servidor. Para solucionar este problema, tuvimos que configurar manualmente el socket con parámetros específicos que permitieran el cierre adecuado de las conexiones una vez finalizadas. Esto no solo evitó que las direcciones quedaran bloqueadas o inutilizables, sino que también optimizó el uso de los recursos del sistema y garantizó un manejo eficiente de las conexiones en el futuro.

Esta configuración fue clave para asegurar que las direcciones quedaran disponibles para ser reutilizadas sin provocar conflictos ni ralentizar el servidor, permitiendo una gestión más eficiente de los recursos de red.
Finalmente, enfrentamos un tercer reto significativo: el servidor se iniciaba correctamente, pero no teníamos un mecanismo para detener los procesos una vez que el servidor estaba en funcionamiento.

### Video - Explicacion

https://youtu.be/zZ7pqyRhiCs

### Conclusion

En conclusión, este proyecto nos permitió superar desafíos clave en la asignación de direcciones IP mediante DHCP. Implementamos Mutex para organizar los registros y evitar la superposición de información, lo que mejoró la claridad del log. Además, configuramos los sockets para liberar adecuadamente las direcciones IP tras su uso, optimizando la reutilización de recursos y evitando la saturación del servidor.

Finalmente, implementamos un mecanismo para detener los procesos del servidor de manera eficiente. Esto, junto con la inclusión de un sistema básico de resolución de nombres DNS, garantiza una gestión estable y eficiente del sistema de red.

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
