# Protocolo DHCP Servidor y Cliente

## Introducción

El **DHCP (Dynamic Host Configuration Protocol)** es un protocolo de red utilizado para asignar automáticamente direcciones IP y otros parámetros de configuración a dispositivos en una red. Su principal objetivo es simplificar la administración de direcciones IP, garantizando que cada dispositivo tenga una dirección IP única sin necesidad de configurarlas manualmente. Este sistema es ampliamente utilizado en redes locales (LAN) y resulta esencial para grandes infraestructuras como campus universitarios, oficinas o incluso redes domésticas con múltiples dispositivos.

En nuestro proyecto, implementamos un servidor DHCP capaz de confirmar la asignación de direcciones IP a través de mensajes **DHCP ACK** y liberar estas direcciones una vez que han expirado o han sido rechazadas. Además, hemos integrado un sistema básico de resolución de nombres mediante **DNS**.

## Detalles Técnicos

### Puertos utilizados por DHCP

El protocolo DHCP utiliza dos puertos bien definidos:
- **67/UDP**: Se utiliza en el lado del servidor DHCP para recibir solicitudes de los clientes. Este puerto está reservado para escuchar mensajes de descubrimiento (DHCP Discover) y solicitudes de configuración (DHCP Request).
- **68/UDP**: Es el puerto utilizado por los clientes DHCP. Aquí es donde el cliente escucha las respuestas del servidor, como la oferta (DHCP Offer) y la confirmación (DHCP ACK).

### ¿Qué es un "DHCP Payload"?

El **DHCP Payload** es el conjunto de datos transmitidos en los mensajes entre el cliente y el servidor. Cada mensaje DHCP tiene un payload que contiene varios campos y opciones que el servidor y el cliente utilizan para comunicarse. Uno de los mensajes más comunes es el **DHCPREQUEST**, que es el mensaje que envía un cliente solicitando una dirección IP al servidor DHCP.

En el **DHCPREQUEST**, el cliente:
1. Confirma su intención de usar la dirección IP ofrecida por el servidor.
2. Pide una renovación de su concesión si ya tiene una dirección asignada.
3. Indica los parámetros de configuración adicionales que necesita.

Este mensaje incluye las opciones que indican al servidor qué acción debe tomar. Veamos algunos de los códigos de opciones que aparecen en nuestro ejemplo:

### Explicación de los códigos DHCP

- **Added DNS entry: example.com -> 93.184.216.34**  
  Se ha añadido una entrada DNS que resuelve `example.com` a la dirección IP `93.184.216.34`. El servidor puede proporcionar estas entradas DNS a los clientes para que puedan traducir nombres de dominio a direcciones IP.

- **Handling DHCP Discover**  
  El servidor ha recibido un mensaje **DHCP Discover** de un cliente, lo que significa que el cliente está buscando servidores DHCP disponibles en la red para solicitar una dirección IP.

- **Adding DHCP option: Code 53, Length 1**  
  La **opción 53** es la **opción de tipo de mensaje DHCP**. Aquí se está añadiendo el tipo de mensaje, que podría ser uno de los siguientes valores:
  - `1`: DHCP Discover
  - `2`: DHCP Offer
  - `3`: DHCP Request
  - `4`: DHCP Decline
  - `5`: DHCP ACK
  - `6`: DHCP NAK
  - `7`: DHCP Release
  - `8`: DHCP Inform

- **Adding DHCP option: Code 51, Length 4**  
  La **opción 51** es el **tiempo de concesión** (lease time), que especifica cuánto tiempo el cliente puede usar la dirección IP asignada. El valor suele estar en segundos, y el servidor envía esta opción para definir el periodo de validez de la dirección IP.

- **Adding DHCP option: Code 54, Length 4**  
  La **opción 54** es la **identificación del servidor DHCP**. El servidor DHCP añade esta opción para indicar su propia dirección IP, lo que ayuda al cliente a saber qué servidor está gestionando la asignación.

- **Adding DHCP option: Code 1, Length 4**  
  La **opción 1** es la **máscara de subred**. El servidor DHCP envía la máscara de subred que el cliente debe utilizar para determinar qué parte de la dirección IP pertenece a la red y qué parte corresponde a los hosts.

- **Adding DHCP option: Code 3, Length 4**  
  La **opción 3** es la **puerta de enlace predeterminada** (default gateway). El servidor DHCP indica al cliente la dirección IP del router o puerta de enlace que debe utilizar para enviar tráfico fuera de su red local.

- **Adding DHCP option: Code 6, Length 4**  
  La **opción 6** es la **dirección del servidor DNS**. El servidor DHCP proporciona la dirección del servidor DNS que el cliente debe utilizar para resolver nombres de dominio a direcciones IP.

- **Sending DHCP Offer to client**  
  El servidor está respondiendo al cliente con un mensaje **DHCP Offer**, ofreciendo una dirección IP junto con los parámetros de configuración de red solicitados.

---

## Conceptos Clave

### Mutex

Un **mutex** (abreviatura de *mutual exclusion*) es una técnica utilizada para evitar que múltiples hilos accedan simultáneamente a un recurso compartido, en este caso, el **log** del servidor. En nuestro proyecto, se utiliza un mutex para asegurarse de que los datos del registro (log) se escriban de manera secuencial, evitando superposiciones cuando varios hilos intentan escribir al mismo tiempo.

### Sockets

Un **socket** es un punto de comunicación entre dos dispositivos en una red. En nuestro servidor DHCP, los sockets se utilizan para escuchar y responder a solicitudes de los clientes. Configuramos los sockets para permitir que las conexiones se cierren correctamente una vez finalizadas, evitando que las direcciones IP queden bloqueadas o inutilizables. Esto garantiza que el servidor pueda gestionar eficientemente las conexiones y reutilizar las direcciones IP.


### Concurrencia

**Concurrencia** es la capacidad de un sistema para ejecutar múltiples tareas o procesos al mismo tiempo. En el contexto de redes y servidores, la concurrencia permite que el servidor maneje múltiples solicitudes de clientes simultáneamente sin bloquearse. Esto se logra mediante la creación de **hilos** o **procesos** separados, lo que permite al sistema gestionar múltiples conexiones en paralelo. En nuestro proyecto, se implementaron **hilos** junto con **mutexes** para asegurar que los recursos compartidos, como el log del servidor o las direcciones IP, sean gestionados de forma segura y sin interferencia entre procesos.

### Arquitectura Cliente-Servidor

La **arquitectura cliente-servidor** es un modelo de diseño de software en el que dos partes interactúan: el **cliente**, que realiza solicitudes de recursos o servicios, y el **servidor**, que provee estos recursos o servicios. En el caso de nuestro servidor DHCP, los **clientes** son dispositivos en la red que solicitan configuraciones como direcciones IP, y el **servidor** DHCP es el encargado de responder a estas solicitudes, ofreciendo y asignando los recursos solicitados (direcciones IP, máscaras de subred, servidores DNS, etc.). Esta arquitectura es fundamental en sistemas distribuidos, ya que permite la escalabilidad y la gestión eficiente de múltiples conexiones de clientes.



Puedes integrarlos en el lugar que consideres adecuado dentro del README.

## Desarrollo y Desafíos

El desarrollo de este proyecto presentó varios desafíos técnicos que logramos superar con éxito:

1. **Concurrencia y superposición en el log**:  
   Durante las pruebas de concurrencia, observamos que los registros de los usuarios se superponían en el log, lo que dificultaba la interpretación de la información. Para resolver este problema, implementamos mecanismos de sincronización mediante **Mutex**, lo que permitió estructurar los logs de manera clara y ordenada, mejorando significativamente la legibilidad de los registros.

2. **Gestión de direcciones y recursos del sistema**:  
   Al asignar direcciones IP, estas no se liberaban correctamente después de ser utilizadas, lo que generaba un consumo excesivo de recursos y afectaba el rendimiento del servidor. La solución implicó la configuración manual de los sockets, añadiendo parámetros que permitieran cerrar adecuadamente las conexiones una vez finalizadas. Esta optimización evitó que las direcciones quedaran bloqueadas o inutilizables, garantizando un uso eficiente de los recursos del sistema y la posibilidad de reutilizar las direcciones sin conflictos.

3. **Compatibilidad con diferentes sistemas operativos**:  
   Inicialmente, intentamos ejecutar el servidor en **Windows** mediante **WSL** y **Visual Studio**, pero encontramos que algunos puertos estaban bloqueados. Decidimos migrar el proyecto a **Linux**, ya que este sistema operativo permite una mayor flexibilidad en la configuración y personalización de servidores. Gracias a ello, Podemos reemplazar el servidor DHCP predeterminado del sistema por el nuestro, lo cual no era posible en (Windows es una basura)[https://theconversation.com/one-small-update-brought-down-millions-of-it-systems-around-the-world-its-a-timely-warning-235122] debido a sus restricciones y su mal diseño en comparacion a linux.

4. **Finalización de procesos en el servidor**:  
   Un tercer desafío importante fue la falta de un mecanismo eficiente para detener los procesos del servidor una vez en funcionamiento. Dado que el hilo principal se quedaba bloqueado, no era posible finalizar las tareas de manera adecuada sin esperar a que este se desbloqueara. Implementamos un mecanismo para gestionar correctamente la finalización de los procesos, lo que permitió un control más eficiente del servidor.

## Video - Explicación

Puedes encontrar una explicación detallada del proyecto en el siguiente video:  
[Explicación DHCP](https://youtu.be/zZ7pqyRhiCs)

## Conclusión

Este proyecto nos permitió profundizar en la gestión de direcciones IP a través de DHCP, enfrentándonos a diversos desafíos técnicos. La implementación de **Mutex** mejoró la claridad de los registros, mientras que la configuración adecuada de los sockets optimizó la reutilización de recursos. Además, logramos superar las limitaciones impuestas por la plataforma Windows al migrar el proyecto a Linux, lo que nos proporcionó mayor control sobre el servidor.

Finalmente, el mecanismo de finalización de procesos, junto con el sistema de resolución DNS integrado, garantiza una gestión estable y eficiente de las conexiones de red.

---



## Instrucciones de ejecución

### Compilar y ejecutar el servidor en C

```bash
gcc -o dhcp_server test_server.c -lpthread
sudo ./dhcp_server
```

### Compilar y ejecutar el cliente en C

```bash
gcc -o dhcp_c test.c -lpthread
sudo ./dhcp_c
```

## Referencias

- [Ejemplo de DHCP server en C](https://github.com/ejt0062/dhcpserver-c/tree/master)
- [Proyecto DHCP en C](https://github.com/crossbowerbt/dhcpserver)
- [Servidor DHCP básico](https://github.com/playma/simple_dhcp)
- [Wikipedia](https://en.wikipedia.org/wiki/Dynamic_Host_Configuration_Protocol)




