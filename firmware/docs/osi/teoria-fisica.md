En las siete capas del modelo OSI de la red informática,[1] el nivel físico o capa física (Capa 1) se refiere a las transformaciones que se le hacen a la secuencia de bits para transmitirlos de un lugar a otro. Esta capa puede ser implementada por un PHY. Siempre los bits se manejan dentro del PC como niveles eléctricos. Por ejemplo, puede decirse que en un punto del cable existe un 1 cuando hay presente un determinado nivel de voltaje y un cero cuando su nivel es de 0 voltios. Cuando se transmiten los bits siempre se transforman en otro tipo de señales de tal manera que en el punto receptor puede recuperar la secuencia de bits originales.


La Capa Física o Nivel 1 proporciona los medios mecánicos, eléctricos, funcionales y de procedimiento para activar, mantener y desactivar conexiones físicas.[2]



Medios de transmisión
Artículo principal: Medio de transmisión
El medio de transmisión constituye el canal que permite la transmisión de información entre dos terminales en un sistema de comunicación.

Las transmisiones se realizan habitualmente empleando medios físicos y ondas electromagnéticas, las cuales se vuelven susceptibles al ser transmitidas por el vacío.

Entramado
La capa física le proporciona servicios a la capa de enlaces de datos con el objetivo que esta le proporcione servicios a la capa de red. La capa física recibe un flujo de bits e intenta enviarlo al destino, no siendo su responsabilidad entregarlos libre de errores. La capa de enlace de datos es la encargada de detectar y corregir los errores. Los errores pueden consistir en una mayor o menor cantidad de bits recibidos o diferencias en los valores que se emitieron y en los que se recibieron.

Un método común de detección de errores es que la capa de enlace de datos separe el flujo en tramas separadas y que realice la suma de verificación de cada trama. Cuando una trama llega a su destino se recalcula la suma de verificación. Si es distinta de la contenida en la trama es porque ha ocurrido un error y la capa de enlace debe solucionarlo.

Funciones y servicios de la capa
Las principales funciones y servicios realizados por la capa física son:

Envío bit a bit entre nodos
Proporcionar una interfaz estandarizada para los medios de transmisión físicos, incluyendo:
Especificaciones mecánicas de los conectores eléctricos y cables, por ejemplo longitud máxima del cable
Especificación eléctrica de la línea de transmisión, nivel de señal e impedancia
Interfaz radio, incluyendo el espectro electromagnético, asignación de frecuencia y especificación de la potencia de señal, ancho de banda analógico, etc.
Especificaciones para IR sobre fibra óptica o una conexión de comunicación wireless mediante IR
Modulación
Codificación de línea
Sincronización de bits en comunicación serie sincrónica
Delimitación de inicio y final, y control de flujo en comunicación serie asíncrona
Multiplexación de Conmutación de circuitos
Detección de portadora y detección de colisión utilizada por algunos protocolos de acceso múltiple del nivel 2
Ecualización, filtrado, secuencias de prueba, conformación de pulso y otros procesados de señales de las señales físicas
La capa física se ocupa también de:

Configuración de la línea punto a punto, multipunto o punto a multipunto
Topología física de la red, por ejemplo en bus, anillo, malla o estrella
Comunicación serie o paralela
Modo de transmisión Simplex, half duplex o full duplex
Subcapa de señalización física
En una red de área local (LAN) o en una red de área metropolitana (MAN) que usa la arquitectura OSI, la subcapa de señalización física es la parte de la capa física que:

se relaciona con la subcapa MAC que es una parte de la capa de Enlace de Datos
realiza la codificación de caracteres, la transmisión, la recepción y decodificación
Fuente: Estándar Federal 1037C

Ejemplos
Ejemplos de protocolos
V.92 red telefónica módems
xDSL
IrDA capa física
USB capa física
Firewire
EIA RS-232, EIA-422, EIA-423, RS-449, RS-485
ITU Recomendaciones: ver ITU-T
DSL
ISDN
T1 y otros enlaces T-carrier, y E1 y otros enlaces E-carrier
10BASE-T, 10BASE2, 10BASE5, 100BASE-TX, 100BASE-FX, 100BASE-T, 1000BASE-T, 1000BASE-SX y otras variedades de la capa física de Ethernet
SONET/SDH
GSM interfaz radio
Bluetooth capa física
IEEE 802.11x Wi-Fi capas físicas
Ejemplos de equipos Hardware
Repetidor
Hub Ethernet
Módem
Nota: Capa física Asociado con la transmisión de cadenas de bits sin estructura sobre un enlace físico. Responsable de las características mecánicas, eléctricas y procedurales que establecen, mantienen y desactivan el enlace físico.