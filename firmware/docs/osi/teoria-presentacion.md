El nivel de presentación o capa de presentación es el sexto nivel del Modelo OSI,[1] y es el que se encarga de la representación de la información, de manera que aunque distintos equipos puedan tener diferentes representaciones internas de caracteres (ASCII, Unicode, EBCDIC), números (little-endian tipo Intel, big-endian tipo Motorola), sonido o imágenes, los datos lleguen de manera reconocible.

Esta capa es la primera en trabajar más el contenido de la comunicación que cómo se establece la misma. En ella se tratan aspectos tales como la semántica y la sintaxis de los datos transmitidos, ya que distintas computadoras pueden tener diferentes formas de manejarlas.[2]

Por lo tanto, podemos resumir definiendo a esta capa como la encargada de manejar las estructuras de datos abstractas y realizar las conversiones de representación de datos necesarias para la correcta interpretación de los mismos.

Esta capa también permite cifrar los datos y comprimirlos. Actúa como traductor.

La Capa 6, o capa de presentación, cumple tres funciones principales. Estas funciones son las siguientes:

Formateo de datos: Significa operar como traductor entre dos tipos diferentes de códigos, cuando dos sistemas diferentes emplean distintos códigos y es indispensable que se comuniquen y su representación en pantalla sea la misma.
Cifrado de datos: El cifrado de datos protege la información durante la transmisión. Se utilizan claves (llaves) de cifrado para cifrar los datos en el lugar de origen y luego descifrarlos en el lugar destino.
Compresión de datos:La compresión de datos funciona mediante el uso de algoritmos para reducir el tamaño de los archivos. El algoritmo busca patrones de bits repetidos en el archivo y entonces los reemplaza con un token. Un token es un patron de bit más corto que representa el patrón largo.
La información que transmiten los datos puede ser de 3 tipos:

Redundante: Información repetitiva o predecible.
Irrelevante: Información que no se aprecia y cuya eliminación por tanto no afecta al contenido del mensaje.
Básica: Es relevante y debe ser transmitida para que se pueda reconstruir la señal.
Para comprender cómo funciona el formateo de datos, tenemos dos sistemas diferentes. El primer sistema utiliza el Código ampliado de caracteres decimal codificados en binario (EBCDIC) para representar los caracteres en la pantalla. El segundo sistema utiliza el Código americano normalizado para el intercambio de la información (ASCII) para la misma función. La Capa 6 opera como traductor entre estos dos tipos diferentes de códigos.

El cifrado de los datos protege la información durante la transmisión. Las transacciones financieras utilizan el cifrado para proteger la información confidencial que se envía a través de Internet. Se utiliza una clave de cifrado para cifrar los datos en el lugar origen y luego descifrarlos en el lugar destino.

Existen dos tipos de cifrado:

Cifrado simétrico: Consiste en el uso una única clave para cifrar y descifrar, esta es conocida como clave privada y tiene como característica principal que es robusta.
Cifrado asimétrico: Consiste en el uso de dos claves, una se usa para cifrar (clave pública) y otra para descifrar (clave privada).
La compresión funciona mediante el uso de algoritmos para reducir el tamaño de los archivos. El algoritmo busca patrones de bits repetidos en el archivo y entonces los reemplaza con un token. Un token es un patrón de bit mucho más corto que representa el patrón largo. Una analogía sencilla puede ser el nombre Rafa (el apodo), el token, para referirse a alguien cuyo nombre completo sea Rafael.

---

Endianness

El término inglés endianness (en español extremidad y a veces endianidad) designa el formato en el que se almacenan los datos de más de un byte en un ordenador. El problema es similar a los idiomas en los que se escriben de derecha a izquierda, como el árabe, o el hebreo, frente a los que se escriben de izquierda a derecha, pero trasladado de la escritura al almacenamiento en memoria de los bytes.

Definición
No se debe confundir trivialmente el orden de escritura textual en este artículo con el orden de escritura en memoria, por ello establecemos que lo que escribimos primero lleva índices de memoria más bajos, y lo que escribimos a continuación lleva índices más elevados; que lo que lleva índices bajos es previo en memoria, y así sucesivamente, siguiendo la ordenación natural de menor a mayor. Por ejemplo, la secuencia {0,1,2} indicaría ―algo más allá de la intuición― que 0 es previo y contiguo en el espacio de memoria a 1, etc.

Usando este criterio, el sistema big endian, adoptado por Motorola, Inc. y otros, consiste en representar los bytes en el orden «natural»: así, el valor hexadecimal 0x4A3B2C1D se codificaría en memoria en la secuencia {4A, 3B, 2C, 1D}. En el sistema little endian, adoptado por Intel, entre otros, el mismo valor se codificaría como {1D, 2C, 3B, 4A}, de manera que se hace más intuitivo el acceso a datos, pues se efectúa fácilmente de manera incremental, de menos relevante a más relevante (siempre se opera con incrementos de contador en la memoria), en un paralelismo a «lo importante no es cómo empiezan las cosas, sino cómo acaban».

Algunas arquitecturas de microprocesador pueden trabajar con ambos formatos (ARM, PowerPC, DEC Alpha, PA-RISC, Arquitectura MIPS). Estas son, a veces, denominadas «sistemas middle endian».

Etimología
La nomenclatura de los criterios little endian y big endian proviene de la novela Los viajes de Gulliver, de Jonathan Swift, que describe a una sociedad compuesta por dos grupos enemistados: uno sostiene que los huevos duros se tienen que empezar a comer por el extremo grande (big end); el otro, por el pequeño (little end). De ahí que big endian se deba entender como «comenzar por el extremo mayor» y little endian como «comenzar por el extremo menor», en vez de entenderse ―como suele ocurrir― con «acaba en grande» y «acaba en pequeño». Su etimología proviene de un juego de palabras en inglés con los términos compuestos little-end-in y big-end-in.[1]

Detección
Un código simple en lenguaje C para detectar si una máquina es little-endian o big-endian:

#include <stdio.h>
#include <stdint.h>

int main(void)
{
   int16_t i = 1;
   int8_t *p = (int8_t *) &i;

   if (p[0] == 1) printf("Little endian\n");
   else printf("Big endian\n");
}
La explicación de su funcionamiento es sencilla: primero se obtiene la dirección de memoria de un entero de valor 1 (0x0001 en hexadecimal), que está compuesto por 16 bits y por lo tanto dos bytes: el de valor 0x00 tiene mayor significancia numérica y el 0x01 menor. Lo que hacemos entonces es leer (desde memoria) solamente el primer byte del mismo (de ahí la conversión a int8_t*), y, si es 0 (0x00), entonces la ordenación es de comienzo por el extremo mayor (big endian). Si es 1 (0x01), es de comienzo por el extremo menor (little endian).