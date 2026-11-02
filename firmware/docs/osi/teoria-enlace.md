El nivel de enlace de datos (en inglés: data link level), o capa de enlace de datos, es la segunda capa del modelo OSI,[1] es responsable de la transferencia fiable de información a través de un circuito de transmisión de datos.[2] Recibe peticiones de la capa de red y utiliza los servicios de la capa física.

El objetivo de la capa de enlace es conseguir que la información fluya, libre de errores, entre dos máquinas que estén conectadas directamente (servicio orientado a la conexión). Para lograr este objetivo tiene que montar bloques de información (llamados tramas en esta capa), dotarles de una dirección de capa de enlace (Dirección MAC), gestionar la detección o corrección de errores, y ocuparse del “control de flujo” entre equipos (para evitar que un equipo más rápido desborde a uno más lento).

Cuando el medio de comunicación está compartido entre más de dos equipos es necesario arbitrar el uso del mismo. Esta tarea se realiza en la subcapa de control de acceso al medio.

Dentro del grupo de normas IEEE 802, la subcapa de enlace lógico se recoge en la norma IEEE 802.2 y es común para todos tipos de redes (Ethernet o IEEE 802.3, IEEE 802.11 o Wi-Fi, IEEE 802.16 o WiMAX, etc.); todas ellas especifican una subcapa de acceso al medio así como una capa física distinta.

Otro tipo de protocolos de la capa de enlace son: protocolo punto a punto (Point-to-Point Protocol, PPP); protocolo de enlace de alto nivel (High-level Data Link Control, HDLC), entre otros.

En la práctica la subcapa de acceso al medio suele formar parte de la propia tarjeta de comunicaciones, mientras que la subcapa de enlace lógico estaría en el programa adaptador de la tarjeta (driver).

Tramas
Una trama es un paquete estructurado de datos que contiene tanto la información que se quiere enviar como los datos de control necesarios para una transmisión organizada, segura y eficiente en la capa de enlace de datos.

Las tramas organizan el flujo de bits en una estructura que permite a los dispositivos comunicarse de manera confiable a través de un enlace físico. La capa de enlace utiliza las tramas para asegurar que los datos se transfieran sin errores entre dispositivos conectados directamente, gestionando aspectos de sincronización, reconocimiento, control de flujo y detección de corrupción de datos.

Una trama es un bloque o paquete de datos que contiene no solo la carga útil (los datos reales que se quieren enviar desde la capa de red), sino también información de control importante para la transmisión, como direcciones de origen y destino (direcciones físicas), marcas que delimitan el inicio y el fin de la trama, y mecanismos de detección de errores como CRC (Cyclic Redundancy Check).

Funciones
La capa de enlace de datos es responsable de la transferencia fiable de información a través de un circuito eléctrico de transmisión de datos. La transmisión de datos lo realiza mediante tramas que son las unidades de información con sentido lógico para el intercambio de datos en la capa de enlace. También hay que tener en cuenta que en el modelo TCP/IP se corresponde a la primera capa.

Sus principales funciones son:

Iniciación, terminación e identificación.
Segmentación y bloqueo.
Sincronización de octeto y carácter.
Delimitación de trama y transparencia.
Control de errores.
Control de flujo.
Recuperación de fallos.
Gestión y coordinación de la comunicación.
Iniciación, terminación e identificación
La función de iniciación comprende los procesos necesarios para activar el enlace e implica el intercambio de tramas de control con el fin de establecer la disponibilidad de las estaciones para transmitir y recibir información.

Las funciones de terminación son de liberar los recursos ocupados hasta la recepción/envío de la última trama. También de usar tramas de control. La identificación es para saber a qué terminal se debe de enviar una trama o para conocer quién envía la trama. Se lleva a cabo mediante la dirección de la capa de enlace.

Segmentación y Agrupación
La segmentación surge por la longitud de las tramas ya que si es muy extensa, se debe de realizar tramas más pequeñas con la información de esa trama excesivamente larga.

Si estas tramas son excesivamente cortas, se ha de implementar unas técnicas de bloque que mejoran la eficiencia y que consiste en concatenar varios mensajes cortos de nivel superior en una única trama de la capa de enlace más larga.

Sincronización de octeto y carácter
En las transferencias de información en la capa de enlace es necesario identificar los bits y saber qué posición les corresponde en cada carácter u octeto dentro de una serie de bits recibidos.

Esta función de sincronización comprende los procesos necesarios para adquirir, mantener y recuperar la sincronización de carácter u octeto. Es decir, poner en fase los mecanismos de codificación del emisor con los mecanismos de decodificación del receptor.

Delimitación de trama y transparencia
La capa de enlace debe ocuparse de la delimitación y sincronización de la trama. Para la delimitación se puede usar tres métodos:

El primero de ellos es "principio y fin" (caracteres específicos para identificar el principio o el fin de cada trama).
También puede usar "principio y cuenta" (utiliza un carácter para indicar comienzo y seguido por un contador que indica su longitud).
Por último puede usar el "guion" (se emplea una agrupación específica de bits para identificar el principio y fin mediante banderas o flags).
La “transparencia” se realiza mediante la ‘inserción de bits’. Consta de ir contando los unos consecutivos y cuando se encuentra con cinco caracteres "1" seguidos y consecutivos (11111) introduce el bit "0" después del quinto 1. Ejemplo: considere la trama 0101111110, al aplicar la transparencia pasa a ser 01011111010.

Control de errores
Proporciona detección y corrección de errores en el envío de tramas entre computadoras, y provee el control de la capa física. Sus funciones, en general, son:

Identificar trama de datos;
Códigos detectores y correctores de error;
Control de flujo;
Gestión y coordinación de la comunicación.
Correctores de error : es opcional en esta capa, la encargada de realizar esta función es la capa de transporte, en una WAN es muy probable que la verificación, la realiza la capa de enlace.

Para la identificación de tramas puede usar distintas técnicas como:

Contador de caracteres
Caracteres de inicio y final con caracteres de relleno
Secuencia de bits indicadora de inicio y final, con bits de relleno
El control de flujo es necesario para no 'agobiar' al receptor. Se realiza normalmente en la capa de transporte, también a veces en la capa de enlace. Utiliza mecanismos de retroalimentación. Suele ir unido a la corrección de errores y no debe limitar la eficiencia del canal.

Los métodos de control de errores son básicamente dos:

Corrección de errores hacia adelante (Forward Error Correction, FEC) o corrección de errores por anticipado, no tiene control de flujo.
Petición de repetición automática (Automatic Repeat-reQuest, ARQ): posee control de flujo mediante parada y espera, y/o ventana deslizante.
Las posibles implementaciones son:

Parada y espera simple: el emisor envía la trama y espera una señal del receptor para enviar la siguiente trama o la que acaba de enviar en caso de error.
Envío continuo y rechazo simple: el emisor envía continuamente tramas y el receptor las va validando. Si encuentra una trama errónea, elimina todas las posteriores y pide al emisor que envíe a partir de la trama errónea.
Envío continuo y rechazo selectivo: transmisión continua salvo que solo retransmite la trama defectuosa.
La detección de errores la realiza mediante diversos tipos de códigos del que hay que resaltar:

Control de redundancia cíclica (CRC)
Simple paridad
Paridad cruzada (Paridad horizontal y vertical)
Suma de verificación
La corrección de errores están basados en Código Hamming, por repetición, verificación de paridad cruzada, Reed-Solomon y de Goyle.

Control de flujo
El control de flujo es necesario para no saturar al receptor de uno a más emisores. Se realiza normalmente en la capa de transporte, también a veces en la capa de enlace. Utiliza mecanismos de retroalimentación. Suele ir unido a la corrección de errores y no debe limitar la eficiencia del canal. El control de flujo conlleva dos acciones importantísimas que son la detección de errores y la corrección de errores.

La detección de errores se utiliza para detectar errores a la hora de enviar tramas al receptor e intentar solucionarlos. Se realiza mediante diversos tipos de códigos del que hay que resaltar el CRC, simple paridad (puede ser par, números de "1" par, o impar), paridad cruzada (Paridad horizontal y vertical) y Suma de verificación.

La corrección de errores surge a partir de la detección para corregir errores detectados y necesitan añadir a la información útil un número de bits redundantes bastante superior al necesario para detectar y retransmitir. Sus técnicas son variadas. El Código Hamming, Repetición, que cada bit se repite tres veces y en caso de fallo se toma el bit que más se repite; También puede hacerse mediante verificación de paridad cruzada, Reed-Solomon y de goyle.

También cabe destacar los protocolos HDLC que es un control de enlace de datos a alto nivel, orientado a bit y obedece a una ARQ de ventana deslizante o continuo. También existen protocolos orientados a carácter.

Recuperación de fallos
Se refiere a los procedimientos para detectar situaciones y recuperar al nivel de situaciones anómalas como la ausencia de respuesta, recepción de tramas inválidas, etc. Las situaciones más típicas son la pérdida de tramas, aparición de tramas duplicadas y llegada de tramas fuera de secuencia.

Si no se tratasen correctamente estos eventos se perderá información y se aceptarán datos erróneos como si fuesen correctos. Generalmente se suelen utilizar contadores para limitar el número de errores o reintentos de los procesos y procedimientos. También se pueden usar temporizadores para establecer plazos de espera (timeout) de los sucesos.

Gestión y coordinación de la comunicación

Gestión y coordinación [imagen]
La gestión atiende a dos tipos:

El primero de ellos es un sistema centralizado donde existe una máquina maestra y varias esclavas. Estas conexiones se pueden realizar punto a punto o multipunto.
El segundo de ellos es el distribuido, donde no existe máquina maestra y todas compiten por el control del sistema de comunicación.
La coordinación se puede realizar mediante selección o contienda:

La selección se puede implementar mediante sondeo/selección, donde el maestro recoge un mensaje de una secundaria y se la entrega a quien seleccione. También es posible asignando un testigo a una máquina que es la que puede emitir mensajes/tramas. Son típicas las configuraciones Token Ring y Token Bus.
La contienda se basa en que cada ordenador emite su trama/mensaje cuando le apetece. Todos los componentes de la red son tanto emisores como receptores. Son típicos los sistemas ALOHA y CSMA/CD. Hay que tener cuidado con las colisiones.