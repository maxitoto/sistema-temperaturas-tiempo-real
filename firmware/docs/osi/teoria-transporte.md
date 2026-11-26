El nivel de transporte o capa de transporte es el cuarto nivel del modelo OSI,[1] y está encargado de la transferencia libre de errores de los datos entre el emisor y el receptor, aunque no estén directamente conectados, así como de mantener el flujo de la red. Es la base de toda la jerarquía de protocolo. La tarea de esta capa es proporcionar un transporte de datos confiable y económico de la máquina de origen a la máquina destino, independientemente de las de redes físicas en uno.[2] Sin la capa transporte, el concepto total de los protocolos en capas tendría poco sentido.

Servicios
Servicios proporcionados a las capas superiores
La meta final de la capa de transporte es proporcionar un servicio eficiente, confiable y económico a sus usuarios, que normalmente son procesos de la capa de aplicación. Para lograr este objetivo, la capa de transporte utiliza los servicios proporcionados por la capa de red. El firmware o software de la capa de transporte que se encarga del transporte se llama entidad de transporte, la cual puede estar en el núcleo del sistema operativo, en un proceso independiente, en un paquete de biblioteca o en la tarjeta de red.

Hay dos tipos de servicio en la capa de transporte, orientado y no orientado a la conexión. En el servicio orientado a la conexión consta de tres partes: establecimiento, transferencia de datos, y liberación. En el servicio no orientado a la conexión se tratan los paquetes de forma individual.

Es la primera capa que lleva a cabo la comunicación extremo a extremo, y esta condición ya se mantendrá en las capas superiores..

Primitivas del servicio de transporte
Para permitir que los usuarios accedan al servicio de transporte, la capa de transporte debe proporcionar algunas operaciones a los programas de aplicación, es decir, una interfaz del servicio de transporte. Cada servicio de transporte tiene su propia interfaz. Con el propósito de ver los aspectos básicos, en esta sección examinaremos primero un servicio de transporte sencillo y su interfaz.

El servicio de transporte es parecido al servicio en red, pero hay algunas diferencias importantes. La principal, es que, el propósito del servicio de red es modelar el servicio ofrecido por las redes reales, con todos sus problemas. Las redes reales pueden perder paquetes, por lo que generalmente el servicio no es confiable. En cambio, el servicio de transporte (orientado a la conexión) si es confiable. Claro que las redes reales no están libres de errores, pero ese es precisamente el propósito de la capa de transporte: ofrecer un servicio confiable en una red no confiable.

Otra diferencia entre la capa de transporte y la de red es a quien van dirigidos sus servicios. El servicio de red lo usan únicamente las entidades de transporte. Pocos usuarios escriben sus entidades de transporte y pocos usuarios o programas llegan a ver los aspectos internos del servicio de red. En cambio, muchos programas ven primitivas de transporte. En consecuencia el servicio de transporte debe ser adecuado y fácil de usar.

Las primitivas de un transporte sencillo serían:

- LISTEN: Se bloquea hasta que algún proceso intenta el contacto.

- CONNECT: Intenta activamente establecer una conexión.

- SEND: Envía información.

- RECEIVE: Se bloquea hasta que llegue una TPDU de DATOS.

- DISCONNECT: Este lado quiere liberar la conexión.

Y con estas primitivas podemos hacer un esquema sencillo de manejo de conexiones.

Sockets de Berkeley
Este es otro grupo de primitivas de transporte, las primitivas usadas en UNIX para el TCP. En general son muy parecidas a las anteriores pero ofrecen más características y flexibilidad.

Elementos de los protocolos de transporte
El servicio de transporte se implementa mediante un protocolo de transporte entre dos entidades de transporte. En ciertos aspectos, los protocolos de transporte se parecen a los protocolos de red. Ambos se encargan del control de errores, la secuenciación y el control del flujo.

Pero también existen diferencias importantes entre ambas, como los entornos en que operan, la capa transporte necesita el direccionamiento explícito de los destinos, mientras que la capa de red no, otra diferencia es la cantidad de datos, mucho mayor en la capa de transporte.

Direccionamiento
Cuando un proceso desea establecer una conexión con un computador de aplicación remoto, debe especificar a cuál se conectará (¿a quién le llegará el mensaje?). El método que normalmente se emplea es definir direcciones de transporte en las que los procesos pueden estar a la escucha de solicitudes de conexiones. En Internet, estos puntos terminales se denominan puertos, pero usaremos el término genérico de TSAP (Punto de Acceso al Servicio de Transporte). Los puntos terminales análogos de la capa de red se llaman NSAP (Punto de Acceso al Servicio de Red). Las direcciones IP son ejemplos de NSAPS.

Establecimiento de una conexión
El establecimiento de una conexión parece fácil, pero en realidad es sorprendentemente difícil. A primera vista, parecería que es suficiente con mandar una TPDU (Unidad de Datos del Protocolo de Transporte) con la petición de conexión y esperar a que el otro acepte la conexión. El problema viene cuando la red puede perder, almacenar, o duplicar paquetes. El principal problema es la existencia de duplicados retrasados. Esto puede solucionarse de varias maneras (ninguna es muy satisfactoria). Una es utilizar direcciones de transporte desechables. En este enfoque cada vez que necesitemos una dirección la creamos. Al liberarse la conexión descartamos la dirección y no se vuelve a utilizar. O también asignar una secuencia dentro de los datos transmitidos, pero estos plantean el problema de que si se pierde la conexión perdemos el orden del identificador y ya no funciona. La solución sería más fácil si los paquetes viejos se eliminaran de la subred cada cierto tiempo de vida. Para ello podemos utilizar las siguientes técnicas: Un diseño de subred Restringido. Colocar un contador de saltos en cada paquete. Marcar el tiempo de cada paquete. Pero en la práctica no vale solo con hacer esto sino que tenemos que garantizar que todas las confirmaciones de los paquetes también se eliminan.

Liberación de una conexión
La liberación de una conexión es más fácil que su establecimiento. No obstante, hay más escollos de los que uno podría imaginar. Hay dos estilos de terminación de una conexión: liberación asimétrica y liberación simétrica. La liberación asimétrica es la manera en que funciona el mecanismo telefónico: cuando una parte cuelga, se interrumpe la conexión. La liberación simétrica trata la conexión como dos conexiones unidireccionales distintas, y requiere que cada una se libere por separado. La liberación asimétrica es abrupta y puede resultar en la pérdida de datos. Por lo que es obvio que se requiere un protocolo de liberación más refinado para evitar la pérdida de datos. Una posibilidad es usar la liberación simétrica, en la que cada dirección se libera independientemente de la otra. Aquí, un host puede continuar recibiendo datos aun tras haber enviado una TPDU de desconexión.

La liberación simétrica es ideal cuando un proceso tiene una cantidad fija de datos por enviar y sabe con certidumbre cuándo los ha enviado. En otras situaciones, la determinación de si se ha efectuado o no todo el trabajo y se debe terminarse o no la conexión no es tan obvia. Podríamos pensar en un protocolo en el que el host 1 diga:”Ya termine, ¿Terminaste también?”. Si el host 2 responde “Ya termine también. Adiós”, la conexión puede liberarse con seguridad.

Pero no es tan fiable por el problema de que siempre tendremos que esperar la confirmación de los mensajes recibidos y si esta confirmación no llega no libera la conexión y después puede que necesite la confirmación de que llegó la confirmación y entraríamos en un bucle del que no podemos salir.

Podemos hacer que al host 1 si no le llega la confirmación después de N intentos (es que quiere la desconexión), se libere. Esto produce una conexión semiabierta en la que el host 1 está desconectado pero el host 2 no como no le llega la confirmación no se desconecta nunca. Para solucionar esto creamos una regla por la cual si al host 2 no le llega ninguna TPDU durante cierta cantidad de segundos, se libera automáticamente.

Control de Flujo y almacenamiento en paralelo
Respecto de la manera en que se manejan las conexiones mientras están en uso, uno de los aspectos clave es el control de flujo. Se necesita un esquema para evitar que un emisor rápido desborde a un receptor lento. La diferencia principal es que un enrutador por lo regular tiene relativamente pocas líneas, y un host puede tener numerosas conexiones. Esta diferencia hace poco práctico emplear la implementación que se hace en la capa de enlace.

En esta capa lo que se hace es que si el servicio de red no es confiable, el emisor debe almacenar en un buffer todas las TPDUs enviadas, igual que en la capa enlace de datos. Sin embargo, con un servicio de red confiable son posibles otros arreglos. En particular, si el emisor sabe que el receptor siempre tiene espacio de buffer, no necesita tener copias de las TPDUs que envía. Sin embargo, si el receptor no garantiza que se aceptará cada TPDU que llegue, el emisor tendrá que usar buffers de todas maneras. En el último caso, el emisor no puede confiar en la confirmación de recepción de la capa red porque esto solo significa que ha llegado la TPDU, no que ha sido aceptada.

Los Buffers pueden ser de tres tipos, y usaremos cada uno de ellos cuando más nos convenga.

El equilibrio óptimo entre el almacenamiento del buffer en el origen y en el destino depende del tipo de tráfico transportado por la conexión.

Multiplexión
La multiplexión de varias conversaciones en conexiones, circuitos virtuales o enlaces físicos desempeña un papel importante en diferentes capas de la arquitectura de red. En la capa de transporte puede surgir la necesidad de multiplexión por varias razones. Por ejemplo, si en un host solo se dispone de una dirección de red, todas las conexiones de transporte de esa máquina tendrán que utilizarla. Cuando llega una TPDU, se necesita algún mecanismo para saber a cuál proceso asignarla. Esta situación se conoce como multiplexión hacia arriba.

La multiplexión también puede ser útil en la capa transporte para la utilización de circuitos virtuales, que dan más ancho de banda cuando se reasigna a cada circuito una tasa máxima de datos. La solución es abrir múltiples conexiones de red y distribuir el tráfico entre ellas. Esto se denomina multiplexión hacia abajo.

Recuperación de caídas
Si los hosts y los enrutadores están sujetos a caídas, la recuperación es fundamental. Si la entidad de transporte está por entero dentro de los hosts, la recuperación de caídas de red y de enrutadores es sencilla. Si la capa de red proporciona servicio de datagramas, las entidades de transporte esperan pérdida de algunas TPDUs todo el tiempo, y saben cómo manejarla. Si la capa de red proporciona servicio orientado a la conexión, entonces la pérdida de un circuito virtual se maneja estableciendo otro nuevo y sondeando la entidad de transporte remota para saber cuales TPDUs ha recibido y cuales no.

Un problema más complicado es la manera de recuperarse de caídas del host. Al reactivarse, sus tablas están en el estado inicial y no sabe con precisión donde estaba.

En un intento por recuperar su estado previo, el servidor podría enviar una TPDU de difusión a todos los demás host, anunciando que se acaba de caer y solicitando a todos sus clientes que le informen el estado de todas la conexiones abiertas.

Protocolos de transporte de internet
Internet tiene dos protocolos principales en la capa de transporte, uno no orientado a la conexión, UDP, y otros orientados a la conexión, el TCP y SCTP.

UDP (Protocolo de Datagramas de Usuario)
Artículo principal: UDP
El conjunto de protocolos de Internet soporta un protocolo de transporte no orientado a la conexión UDP (protocolo de datagramas de usuario). Este protocolo proporciona una forma para que las aplicaciones envíen datagramas IP encapsulados sin tener una conexión.

Algunas características de este protocolo son:

Genera pocos gastos y se utiliza para aplicaciones que no requieren envío de datos confiable.
No orientado a la conexión, descrito en la RFC 768, sólo posee 8 bytes de carga.
No rastrea la recepción de datagramas en el destino, sólo envía los datagramas recibidos a la capa de aplicación a medida que llega y no reenvía datagramas perdidos. Por esto es más rápido y ligero.
Proporciona un servicio de datagramas poco fiable.
TCP (Protocolo de Control de Transmisión)
Artículo principal: TCP
TCP (protocolo de control de transmisión) se diseñó específicamente para proporcionar un flujo de bytes confiable de extremo a extremo a través de una interred no confiable. Una interred difiere de una sola red debido a que diversas partes podrían tener diferentes topologías, anchos de banda, retardos, tamaños de paquete… TCP tiene un diseño que se adapta de manera dinámica a las propiedades de la interred y que se sobrepone a muchos tipos de situaciones.

El protocolo TCP proporciona un servicio completo y fiable, descrito en la RFC 793. Cada segmento de TCP posee 20 bytes de carga en el encabezado.

Algunas características del protocolo TCP son:

Es un protocolo orientado a la conexión.
Entrega confiable.
Control de flujo.
Utiliza mecanismos de enlace, temporizadores, acuses de recibo y uso dinámico de ventanas.
Su confiabilidad implica cierta sobrecarga en el tamaño de los encabezados y mayor tráfico entre el origen y el destino.
SCTP (Protocolo de Transmisión para el Control de Flujo)
Artículo principal: SCTP
El protocolo SCTP proporciona un servicio completo y fiable.
Ofrece los mismos servicios a las aplicaciones que el protocolo TCP.
Admite conexiones entre sistemas que tienen más de una dirección (host múltiple).
La conexión SCTP entre el sistema transmisor y receptor se denomina asociación.
Los datos de la asociación se organizan en bloques.
DCCP (Protocolo de Control de Congestión de Datagramas)
Artículo principal: DCCP
FCP (Protocolo de Canal de Fibra)
Artículo principal: FCP
