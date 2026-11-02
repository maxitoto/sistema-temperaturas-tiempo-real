El nivel de red o capa de red, según la normalización OSI,[1] es un nivel o capa que proporciona conectividad y selección de ruta entre dos sistemas de hosts que pueden estar ubicados en redes geográficamente distintas. Es el tercer nivel del modelo OSI y su misión es conseguir que los datos lleguen desde el origen al destino aunque no tengan conexión directa. Ofrece servicios al nivel superior (nivel de transporte) y se apoya en el nivel de enlace, es decir, utiliza sus funciones.[2]

Para la consecución de su tarea, puede asignar direcciones de red únicas, interconectar subredes distintas, encaminar paquetes, utilizar un control de congestión y control de errores.

Orientación de conexión
Véase también: Conmutador de paquetes
Hay dos formas en las que el nivel de red puede funcionar internamente, pero independientemente de que la red funcione internamente con datagramas o con circuitos virtuales puede dar hacia el nivel de transporte un servicio orientado a conexión:

Datagramas: Cada paquete se encamina independientemente, sin que el origen y el destino tengan que pasar por un establecimiento de comunicación previo.
Circuitos virtuales: En una red de circuitos virtuales dos equipos que quieran comunicarse tienen que empezar por establecer una conexión. Durante este establecimiento de conexión, todos los routers que haya por el camino elegido reservarán recursos para ese circuito virtual específico.
-La tarea principal de la capa de enlace de datos es tomar una transmisión de datos y transformarla en una extracción libre de errores de transmisión para la capa de red. Logra esta función dividiendo los datos de entrada en marcos de datos (de unos cuantos cientos de bytes), transmite los marcos en forma secuencial y procesa los marcos de estado que envía el nodo destino. Si se habla de tramas es de capa de enlace.

Tipos de servicios
Hay dos tipos de servicio:[3]

Servicios no orientados a la conexión (CLNS): Cada paquete debe llevar la dirección destino, y con cada uno, los nodos de la red deciden el camino que se debe seguir. Existen muchas técnicas para realizar esta decisión, como por ejemplo comparar el retardo que sufriría en ese momento el paquete que se pretende transmitir según el enlace que se escoja dependiendo del tipo de red.
Servicios orientados a la conexión (CONS): Sólo el primer paquete de cada mensaje tiene que llevar la dirección destino. Con este paquete se establece la ruta que deberán seguir todos los paquetes pertenecientes a esta conexión. Cuando llega un paquete que no es el primero se identifica a que conexión pertenece y se envía por el enlace de salida adecuado, según la información que se generó con el primer paquete y que permanece almacenada en cada conmutador o nodo.
Encaminamiento
Artículo principal: Encaminamiento
Encaminamiento (o enrutamiento, ruteo) es la función de buscar un camino entre todos los posibles en una red de paquetes cuyas topologías poseen una gran conectividad. Dado que se trata de encontrar la mejor ruta posible, lo primero será definir qué se entiende por mejor ruta y en consecuencia cuál es la métrica que se debe utilizar para medirla.

Control de congestión
Artículo principal: Congestión de red
Cuando en una red un nodo recibe más tráfico del que puede procesar se puede dar una congestión. El problema es que una vez que se da congestión en un nodo el problema tiende a extenderse por el resto de la red. Por ello hay técnicas de prevención y control que se pueden y deben aplicar en el nivel de red.

Algunos protocolos de la capa de red
Algunos protocolos de la capa de red son:

IP (IPsec, IPv4, IPv6, )
OSPF
IS-IS
ICMP, ICMPv6
IGMP
