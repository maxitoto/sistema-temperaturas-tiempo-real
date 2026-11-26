# capa de sesión
El nivel de sesión o capa de sesión es el quinto nivel del modelo OSI,[1] que proporciona los mecanismos para controlar el diálogo entre las aplicaciones de los sistemas finales. En muchos casos, los servicios de la capa de sesión son parcialmente, o incluso, totalmente prescindibles. No obstante, en algunas aplicaciones su utilización es ineludible.

## La capa de sesión proporciona los siguientes servicios:

Control del Diálogo: Éste puede ser simultáneo en los dos sentidos (full-duplex) o alternado en ambos sentidos (half-duplex).
Agrupamiento: El flujo de datos se puede marcar para definir grupos de datos (paquetes).
Recuperación: La capa de sesión puede proporcionar un procedimiento de puntos de comprobación, de forma que si ocurre algún tipo de fallo entre puntos de comprobación, la entidad de sesión puede retransmitir todos los datos desde el último punto de comprobación y no desde el principio.
Notificación de errores: Mecanismo de propósito general para notificar errores inesperados. Si un usuario tiene algún problema, este problema se puede notificar a su corresponsal.
Todas estas capacidades se podrían incorporar en las aplicaciones de la capa 7. Sin embargo ya que todas estas herramientas para el control del diálogo son ampliamente aplicables, parece lógico organizarlas en una capa separada, denominada capa de sesión.[2]

La capa de sesión surge como una forma de organizar y sincronizar el diálogo y controlar el intercambio de datos.

La capa de sesión permite a los usuarios de máquinas diferentes establecer sesiones entre ellos. Una sesión permite el transporte ordinario de datos, como lo hace la capa de transporte, pero también proporciona servicios mejorados que son útiles en algunas aplicaciones.[3] Se podría usar una sesión para que el usuario se conecte a un sistema remoto de tiempo compartido o para transferir un archivo entre dos máquinas.

## Objetivos
Permitir que los usuarios de diferentes máquinas establezcan sesiones entre ellos
Permitir a un usuario entrar a un sistema de tiempo compartido a distancia o bien, compartir archivos a distancia.
Entre la administración y finalización de las sesiones, se llevan a cabo algunos servicios claves para el correcto funcionamiento, los cuales son:
Control de la sesión.
mantener los puntos de verificación
Control de concurrencia (evitar que interfieran dos procesos al mismo tiempo)
Por lo tanto, esta capa asegura o mantiene el enlace entre dos equipos y permite reanudar tareas en caso de alguna interrupción.
En esta capa se permite el tráfico de la red en ambas direcciones al mismo tiempo o a una sola dirección. Si el tráfico solo va en una dirección, la capa de sesión ayudará en el seguimiento de quien tiene el turno.
Existe la sincronización ya que la capa proporciona una forma de insertar puntos de verificación en el flujo de datos, con el objetivo de que después de cada caída, solamente se repitan los datos que están después del último punto de verificación.
Una sesión se parece a una conexión de transporte.
Cuando llega a presentarse una solicitud para que la capa de sesión establezca una sesión, se deberá establecer una conexión de transporte que se encargue de soportar la conexión
Cuando termina la sesión se libera la conexión de transporte.
Cada vez que un agente contesta una llamada, se establece una sesión con el equipo principal.
Una vez que un agente contesta una llamada, se establece una sesión con el equipo principal.
Una vez que la sesión se procesa, la sesión se da por terminada
Lo importante en este punto es que no hay necesidad de cargar con el problema de liberar la conexión de transporte subyacente.
Protocolos de la capa de sesión
Protocolo RPC (llamada a procedimiento remoto): es un protocolo que permite a un programa de ordenador ejecutar código en otra máquina remota sin tener que preocuparse por las comunicaciones entre ambos. El protocolo es un gran avance sobre los sockets usados hasta el momento. Las RPC son muy utilizadas dentro del paradigma cliente-servidor, siendo el cliente el que inicia el proceso solicitando al servidor que ejecute cierto procedimiento o función, y enviando de vuelta este último el resultado de dicha operación al cliente. Hoy en día se está utilizando el XML como lenguaje para definir el IDL y el HTTP como protocolo de red, dando lugar a lo que se conoce como servicios web.
El cliente hace la llamada al procedimiento remoto mediante un mensaje a través de la red, éste se detiene ya que es un proceso síncrono, es decir, necesita una respuesta del servidor para continuar su ejecución, en esta llamada se incluye un STUN (resguardo) el cual se encarga de ajustar parámetros y direcciones de memoria en un ambiente (cliente) u otro (servidor).
El servidor recibe la petición y desempaqueta el mensaje para extraer la información necesaria para realizar la tarea. El stub ayuda a que el servidor sea capaz de convertir parámetros de una representación a otra, para traducir direcciones de memoria de cliente a servidor.
El servidor ejecuta la tarea.
El servidor crea un mensaje de respuesta para el cliente en el que incluye el resultado de la tarea que este le pidió realizar
El cliente recibe y desempaqueta el mensaje de respuesta del servidor, continúa con su ejecución normal. STUB: El stub es la pieza de código que le permite al servidor ejecutar la tarea que se le asignó, se encarga de proveer la información necesaria para que el servidor convierta las direcciones de los parámetros que el cliente envió en direcciones de memoria válidas dentro del ambiente del servidor. La representación de datos entre el cliente y el servidor podría discrepar, el stub también provee la información necesaria para solucionar esta situación. De manera general, es la pieza de código que se encarga de enmascarar toda la discrepancia entre el cliente y servidor, es necesario que las bibliotecas stubs estén instaladas tanto en el cliente como en el servidor.

## Comparación con el modelo TCP/IP
El modelo TCP/IP no se ocupa de los detalles del modelo OSI de la semántica del protocolo de aplicación o transporte y, por lo tanto, no considera una capa de sesión. La gestión de la sesión de OSI en relación con los protocolos de transporte típicos (TCP, SCTP) está contenida en los protocolos de la capa de transporte, o de lo contrario se considera el ámbito de los protocolos de la capa de aplicación. Las capas de TCP/IP son descripciones de ámbitos operativos (aplicación, host-a-host, red, enlace) y no prescripciones detalladas de procedimientos operativos o semántica de datos.

---

# Llamada a procedimiento remoto
En computación distribuida, la llamada a procedimiento remoto (en inglés, Remote Procedure Call, RPC) es un programa que utiliza una computadora para ejecutar código en otra máquina remota sin tener que preocuparse por las comunicaciones entre ambas, de forma que parezca que se ejecuta en local. El protocolo que se utiliza para esta llamada es un gran avance sobre los sockets de Internet usados hasta el momento. De esta manera el programador no tenía que estar pendiente de las comunicaciones, estando estas encapsuladas dentro de las RPC.

Una llamada a procedimiento es muy similar a una invocación a un método remoto en la que un programa cliente llama a un procedimiento de otro programa en ejecución en un proceso servidor. Los servidores pueden ser clientes de otros servidores para permitir cadenas de RPC. Un proceso servidor define en su interfaz de servicio los procedimientos disponibles para ser llamados remotamente. RPC se implementa usualmente sobre protocolo petición-respuesta, que se encuentra simplificado por omisión de referencias a objetos remotos en la parte de los mensajes de petición.

Las RPC son muy utilizadas dentro de la comunicación cliente-servidor. Siendo el cliente el que inicia el proceso solicitando al servidor que ejecute cierto procedimiento o función y enviando este de vuelta el resultado de dicha operación al cliente.

El cliente que accede a un servicio incluye un procedimiento de resguardo para cada procedimiento en la interfaz de servicio. El papel de un procedimiento de resguardo es similar al de un proxy. Se comporta como un procedimiento local del cliente pero en lugar de ejecutar la llamada, empaqueta el identificador del procedimiento y los argumentos en un mensaje de petición que se envía vía su módulo de comunicación al servidor; cuando llega el mensaje de respuesta, desempaqueta los resultados.

El concepto de RPC fue introducido por primera vez por Birrell y Nelson en 1984, y sentaron las bases para el desarrollo en sistemas distribuidos que se usa actualmente.[1]

Problemas de diseño
Existen problemas asociados a la implementación de los sistemas RPC que son la programación con interfaces y de transparencia, ambos resumidos del libro "Distributed Systems: Concepts and Design".[2]

Interfaces
La mayoría de programas modernos presentan una estructura modular, en la que una función puede ser utilizada mediante una interfaz, la cual enmascara dicho procedimiento pudiéndose cambiar su funcionalidad sin que afecte a la forma en la que es llamada. Estos módulos pueden comunicarse entre sí mediante RPC de tal manera que las interacciones quedan controladas a través de las interfaces de los módulos, aunque varíe su implementación interna.

En un programa distribuido, por ejemplo, con arquitectura cliente-servidor, el servidor proporciona a los clientes un listado de los módulos que están disponibles para su uso, así como los argumentos necesarios para realizar una correcta llamada a los mismos. De esta forma el cliente únicamente debe preocuparse de la interfaz, no necesita saber detalles de la implementación del módulo, como por ejemplo el lenguaje de programación, o la plataforma en la que el servicio fue implementado, de esta manera se soluciona el problema de la heterogeneidad de los sistemas distribuidos actuales.

Por otro lado, si un módulo que se ejecuta en un proceso necesita acceder a variables de otro módulo que se ejecuta en otro proceso, no puede acceder a ellas, ya que el paso de argumentos por referencia no está permitido. Como consecuencia de esto último, los parámetros de salida van en un mensaje de respuesta. Una excepción es CORBA que puede acceder a las variables de un módulo situado en otro proceso mediante los métodos getter y setter.

Transparencia
Los creadores de RPC, Birrell y Nelson, intentaron hacer las llamadas a procedimiento remoto lo más parecidas a las locales, de tal manera que no hubiese una distinción entre una llamada local y una llamada remota. Las transformaciones de la representación de los datos a un formato adecuado, el llamado marshalling, así como otros procedimientos de paso de mensaje se ocultan al programador. RPC se centra en ofrecer transparencia en la localización y el acceso, ocultando la localización física de los métodos.

Sin embargo, las llamadas a procedimientos remotos son más vulnerables a fallar que las locales, ya que implican una conexión a red y otro ordenador con otro proceso. Eso hace que se tenga más dificultad en determinar qué ha podido ocasionar un fallo.

También hay que tener en cuenta que las llamadas RPC tienen una latencia superior. Esto se debe tener en cuenta reduciendo lo mínimo posible este tipo de llamadas o haciendo que el procedimiento que realiza la llamada RPC sea capaz de abortarla en el caso de que tarde demasiado tiempo. Si se hace lo segundo, es importante que el servidor restaure la información a como estaba antes de la llamada.

Existe una discusión entre los programadores de cómo de transparente tiene que ser una llamada RPC. Por ejemplo, los creadores del lenguaje de programación Argus (Liskov y Scheifler) afirmaban que las llamadas RPC deberían ser diferentes que las locales, y como consecuencia de ello en Argus hicieron las llamadas remotas explícitas al programador.[3] La conclusión general a la que se ha llegado es que la sintaxis entre una llamada local y una remota debería ser igual, y que la diferencia entre una y otra debería ser explicada en sus interfaces.

Tipos de semántica
Existen diferentes modos que garantizan la entrega de mensajes en RPC:

Reintentar el mensaje de solicitud:  controla la retransmisión de la petición hasta que el servidor la reciba o suponga que ha fallado
Filtro de duplicados: controla el uso de retransmisiones y si el servidor descarta peticiones duplicadas
Retransmisión de los resultados: Controla si se debe mantener un historial de resultados de mensaje, permitiendo volver a transmitir los resultados perdidos sin volver a ejecutar las operaciones en el servidor.
La combinación de estas opciones conduce a tres posibles tipos de semánticas: “tal-vez”, “al-menos-una-vez” y “como-máximo-una-vez”[4]

Medidas de tolerancia a fallos[4]	Semánticas de llamada
Reintentar el mensaje de solicitud	Filtro de duplicados	Retransmisión de los resultados
No	No es aplicable	No es aplicable	tal vez
Sí	No	Volver a ejecutar el procedimiento	al menos una vez
Sí	Sí	Retransmitir la respuesta	como máximo una vez
Semántica "tal-vez"
Procedimiento remoto puede ejecutarse una vez o ninguna vez.
El cliente puede recibir una respuesta o ninguna.
Funcionamiento
El cliente envía una petición y se queda a la espera un tiempo determinado.
Si no llega la respuesta dentro del tiempo de espera, continúa su ejecución.
El cliente no tiene realimentación en caso de fallo (no sabe que pasó).
Sólo admisible en aplicaciones donde se tolere la pérdida de peticiones y la recepción de respuestas con retraso (fuera de orden).

Semántica "al-menos-una-vez"
Procedimiento remoto se ejecuta una o más veces.
El cliente puede recibir una o más respuestas.
Funcionamiento
El cliente envía una petición y queda a la espera un tiempo.
Si no llega respuesta o ACK dentro del tiempo de espera, repite la petición.
El servidor no filtra peticiones duplicadas (el procedimiento remoto puede ejecutarse repetidas veces).
El cliente puede recibir varias respuestas.
Sólo es aplicable cuando se usan exclusivamente operaciones idempotentes (repetibles). Nota: una operación es idempotente si se puede ejecutar varias veces resultando el mismo efecto que si se hubiera ejecutado sólo una. En ocasiones una operación no idempotente puede implementarse como una secuencia de operaciones idempotentes. Admisible en aplicaciones donde se tolere que se puedan repetir invocaciones sin afectar a su funcionamiento.

Semántica "como-máximo-una-vez"
El procedimiento remoto se ejecuta exactamente una vez o no llega a ejecutarse ninguna.
El cliente recibe una respuesta o una indicación de que no se ha ejecutado el procedimiento remoto.
Funcionamiento
El cliente envía la petición y queda a la espera un tiempo.
Si no llega respuesta o ACK dentro del tiempo de espera, repite la petición.
El servidor filtra las peticiones duplicadas y guarda historial con las respuestas enviadas (servidor con memoria). El procedimiento remoto sólo se ejecuta una vez.
El cliente sólo recibe una respuesta si la petición llegó y se ejecutó el procedimiento, si no recibe informe del error.
Resumen de semánticas RPC
Semántica	Funcionamiento
tal vez	Cliente no retransmite sus peticiones (no usa ACK)
Servidor no filtra peticiones duplicadas
al menos una vez	Cliente retransmite sus peticiones (usa ACK + temporizador)
Servidor no filtra peticiones duplicadas
Ante peticiones repetidas, servidor repite ejecución
como máximo una	Cliente retransmite sus peticiones (usa ACK + temporizador)
Servidor filtra peticiones duplicadas
Ante peticiones repetidas, servidor retransmite las respuestas pasadas
Implementaciones
La implementación de una llamada a procedimiento remoto requiere una serie de componentes de software, los cuales se pueden ver en la siguiente figura traducida de la que se encuentra en el libro "Distributed Systems: Concepts and Design".[2]


Esquema de implementación RPC
Dentro del proceso cliente se encuentra el programa principal (programa cliente). Este se comunica con un procedimiento stub que se encarga de realizar la función de marshaling, para transformar los datos al formato adecuado y enviarlos como un mensaje de solicitud a través del módulo de comunicación. Una vez que la respuesta llega, el stub deshace la transformación para obtener los resultados.

El módulo de comunicación del proceso cliente se comunica con el del proceso servidor, desde el cual transmite los datos a un distribuidor (dispatcher) que hace llegar los datos al procedimiento stub correspondiente, que realizará la función de marshaling con el procedimiento servicio. Al existir varios stub, uno por cada procedimiento de servicio, el distribuidor se encarga de elegir el que corresponda en cada petición.

Los módulos de comunicación no solo se encargan de direccionar los mensajes sino también de otros aspectos relativos a la comunicación como mensajes duplicados, timeouts o retransmisiones.[4]

En general, RPC implementa un protocolo de petición-respuesta.

Las llamadas a procedimiento remoto están implementadas mediante varios tipos de protocolos, muchos de ellos estandarizados como pueden ser el RPC de Sun denominado ONC RPC (RFC 1057), el RPC de Open Software Foundation (OSF) denominado DCE/RPC y el "Modelo de Objetos de Componentes Distribuidos de Microsoft" (Distributed Component Object Model, DCOM), aunque ninguno de estos es compatible entre sí. La mayoría de ellos utilizan un lenguaje de descripción de interfaz (Interface description language o IDL) que define los métodos exportados por el servidor.

Hoy en día se está utilizando el XML como lenguaje para definir el IDL y el HTTP como protocolo de aplicación, dando lugar a lo que se conoce como servicios web. Ejemplos de estos pueden ser SOAP o XML-RPC.

---
RFC 1050


                       RPC: Remote Procedure Call
                         Protocol Specification
                               Version 2

STATUS OF THIS MEMO

   This RFC describes a standard that Sun Microsystems and others are
   using, and is one we wish to propose for the Internet's
   consideration.  This memo is not an Internet standard at this time.
   Distribution of this memo is unlimited.

1. INTRODUCTION

   This document specifies version two of the message protocol used in
   Sun's Remote Procedure Call (RPC) package.  The message protocol is
   specified with the eXternal Data Representation (XDR) language [9].
   This document assumes that the reader is familiar with XDR.  It does
   not attempt to justify remote procedure calls systems or describe
   their use.  The paper by Birrell and Nelson [1] is recommended as an
   excellent background for the remote procedure call concept.

2. TERMINOLOGY

   This document discusses clients, calls, servers, replies, services,
   programs, procedures, and versions.  Each remote procedure call has
   two sides: an active client side that sends the call to a server,
   which sends back a reply.  A network service is a collection of one
   or more remote programs.  A remote program implements one or more
   remote procedures; the procedures, their parameters, and results are
   documented in the specific program's protocol specification (see
   Appendix A for an example).  A server may support more than one
   version of a remote program in order to be compatible with changing
   protocols.

   For example, a network file service may be composed of two programs.
   One program may deal with high-level applications such as file system
   access control and locking.  The other may deal with low-level file
   input and output and have procedures like "read" and "write".  A
   client of the network file service would call the procedures
   associated with the two programs of the service on behalf of the
   client.

   The terms client and server only apply to a particular transaction; a



Sun Microsystems                                                [Page 1]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   particular hardware entity (host) or software entity (process or
   program) could operate in both roles at different times.  For
   example, a program that supplies remote execution service could also
   be a client of a network file service.  On the other hand, it may
   simplify software to separate client and server functionality into
   separate libraries or programs.

3. THE RPC MODEL

   The Sun RPC protocol is based on the remote procedure call model,
   which is similar to the local procedure call model.  In the local
   case, the caller places arguments to a procedure in some well-
   specified location (such as a register window).  It then transfers
   control to the procedure, and eventually regains control.  At that
   point, the results of the procedure are extracted from the well-
   specified location, and the caller continues execution.

   The remote procedure call model is similar.  One thread of control
   logically winds through two processes: the caller's process, and a
   server's process.  The caller process first sends a call message to
   the server process and waits (blocks) for a reply message.  The call
   message includes the procedure's parameters, and the reply message
   includes the procedure's results.  Once the reply message is
   received, the results of the procedure are extracted, and caller's
   execution is resumed.

   On the server side, a process is dormant awaiting the arrival of a
   call message.  When one arrives, the server process extracts the
   procedure's parameters, computes the results, sends a reply message,
   and then awaits the next call message.

   In this model, only one of the two processes is active at any given
   time.  However, this model is only given as an example.  The Sun RPC
   protocol makes no restrictions on the concurrency model implemented,
   and others are possible.  For example, an implementation may choose
   to have RPC calls be asynchronous, so that the client may do useful
   work while waiting for the reply from the server.  Another
   possibility is to have the server create a separate task to process
   an incoming call, so that the original server can be free to receive
   other requests.

   There are a few important ways in which remote procedure calls differ
   from local procedure calls:

   1. Error handling: failures of the remote server or network must be
   handled when using remote procedure calls.

   2. Global variables and side-effects: since the server does not have



Sun Microsystems                                                [Page 2]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   access to the client's address space, hidden arguments cannot be
   passed as global variables or returned as side effects.

   3. Performance:  remote procedures usually operate one or more orders
   of magnitude slower than local procedure calls.

   4. Authentication: since remote procedure calls can be transported
   over insecure networks, authentication may be necessary.

   The conclusion is that even though there are tools to automatically
   generate client and server libraries for a given service, protocols
   must still be designed carefully.

4. TRANSPORTS AND SEMANTICS

   The RPC protocol can be implemented on several different transport
   protocols.  The RPC protocol does not care how a message is passed
   from one process to another, but only with specification and
   interpretation of messages.  On the other hand, the application may
   wish to obtain information about (and perhaps control over) the
   transport layer through an interface not specified in this document.
   For example, the transport protocol may impose a restriction on the
   maximum size of RPC messages, or it may be stream-oriented like TCP
   with no size limit.  The client and server must agree on their
   transport protocol choices, through a mechanism such as the one
   described in Appendix A.

   It is important to point out that RPC does not try to implement any
   kind of reliability and that the application may need to be aware of
   the type of transport protocol underneath RPC.  If it knows it is
   running on top of a reliable transport such as TCP [6], then most of
   the work is already done for it.  On the other hand, if it is running
   on top of an unreliable transport such as UDP [7], it must implement
   its own time-out, retransmission, and duplicate detection policies as
   the RPC layer does not provide these services.

   Because of transport independence, the RPC protocol does not attach
   specific semantics to the remote procedures or their execution
   requirements.  Semantics can be inferred from (but should be
   explicitly specified by) the underlying transport protocol.  For
   example, consider RPC running on top of an unreliable transport such
   as UDP.  If an application retransmits RPC call messages after time-
   outs, and does not receive a reply, it cannot infer anything about
   the number of times the procedure was executed.  If it does receive a
   reply, then it can infer that the procedure was executed at least
   once.

   A server may wish to remember previously granted requests from a



Sun Microsystems                                                [Page 3]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   client and not regrant them in order to insure some degree of
   execute-at-most-once semantics.  A server can do this by taking
   advantage of the transaction ID that is packaged with every RPC
   message.  The main use of this transaction is by the client RPC layer
   in matching replies to calls.  However, a client application may
   choose to reuse its previous transaction ID when retransmitting a
   call.  The server may choose to remember this ID after executing a
   call and not execute calls with the same ID in order to achieve some
   degree of execute-at-most-once semantics.  The server is not allowed
   to examine this ID in any other way except as a test for equality.

   On the other hand, if using a "reliable" transport such as TCP, the
   application can infer from a reply message that the procedure was
   executed exactly once, but if it receives no reply message, it cannot
   assume the remote procedure was not executed.  Note that even if a
   connection-oriented protocol like TCP is used, an application still
   needs time-outs and reconnection to handle server crashes.

   There are other possibilities for transports besides datagram- or
   connection-oriented protocols.  For example, a request-reply protocol
   such as VMTP [2] is perhaps a natural transport for RPC.  The Sun RPC
   package currently uses both TCP and UDP transport protocols, with
   experimentation underway on others such as ISO TP4 and TP0.

5. BINDING AND RENDEZVOUS INDEPENDENCE

   The act of binding a particular client to a particular service and
   transport parameters is NOT part of this RPC protocol specification.
   This important and necessary function is left up to some higher-level
   software.  (The software may use RPC itself; see Appendix A.)

   Implementors could think of the RPC protocol as the jump-subroutine
   instruction ("JSR") of a network; the loader (binder) makes JSR
   useful, and the loader itself uses JSR to accomplish its task.
   Likewise, the binding software makes RPC useful, possibly using RPC
   to accomplish this task.

6. AUTHENTICATION

   The RPC protocol provides the fields necessary for a client to
   identify itself to a service, and vice-versa, in each call and reply
   message.  Security and access control mechanisms can be built on top
   of this message authentication.  Several different authentication
   protocols can be supported.  A field in the RPC header indicates
   which protocol is being used. More information on specific
   authentication protocols is in section 9: "Authentication Protocols".





Sun Microsystems                                                [Page 4]
RFC 1057            Remote Procedure Call, Version 2           June 1988


7. RPC PROTOCOL REQUIREMENTS

   The RPC protocol must provide for the following:

   (1) Unique specification of a procedure to be called.
   (2) Provisions for matching response messages to request messages.
   (3) Provisions for authenticating the caller to service and vice-
       versa.

   Besides these requirements, features that detect the following are
   worth supporting because of protocol roll-over errors, implementation
   bugs, user error, and network administration:

   (1) RPC protocol mismatches.
   (2) Remote program protocol version mismatches.
   (3) Protocol errors (such as misspecification of a procedure's
       parameters).
   (4) Reasons why remote authentication failed.
   (5) Any other reasons why the desired procedure was not called.

7.1 RPC Programs and Procedures

   The RPC call message has three unsigned integer fields -- remote
   program number, remote program version number, and remote procedure
   number -- which uniquely identify the procedure to be called.
   Program numbers are administered by some central authority (like
   Sun).  Once implementors have a program number, they can implement
   their remote program; the first implementation would most likely have
   the version number 1.  Because most new protocols evolve, a version
   field of the call message identifies which version of the protocol
   the caller is using.  Version numbers make speaking old and new
   protocols through the same server process possible.

   The procedure number identifies the procedure to be called.  These
   numbers are documented in the specific program's protocol
   specification.  For example, a file service's protocol specification
   may state that its procedure number 5 is "read" and procedure number
   12 is "write".

   Just as remote program protocols may change over several versions,
   the actual RPC message protocol could also change.  Therefore, the
   call message also has in it the RPC version number, which is always
   equal to two for the version of RPC described here.

   The reply message to a request message has enough information to
   distinguish the following error conditions:

   (1) The remote implementation of RPC does not speak protocol version



Sun Microsystems                                                [Page 5]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   2. The lowest and highest supported RPC version numbers are returned.

   (2) The remote program is not available on the remote system.

   (3) The remote program does not support the requested version number.
   The lowest and highest supported remote program version numbers are
   returned.

   (4) The requested procedure number does not exist.  (This is usually
   a client side protocol or programming error.)

   (5) The parameters to the remote procedure appear to be garbage from
   the server's point of view.  (Again, this is usually caused by a
   disagreement about the protocol between client and service.)

7.2 Authentication

   Provisions for authentication of caller to service and vice-versa are
   provided as a part of the RPC protocol.  The call message has two
   authentication fields, the credentials and verifier.  The reply
   message has one authentication field, the response verifier.  The RPC
   protocol specification defines all three fields to be the following
   opaque type (in the eXternal Data Representation (XDR) language [9]):

         enum auth_flavor {
            AUTH_NULL       = 0,
            AUTH_UNIX       = 1,
            AUTH_SHORT      = 2,
            AUTH_DES        = 3
            /* and more to be defined */
         };

         struct opaque_auth {
            auth_flavor flavor;
            opaque body<400>;
         };

   In other words, any "opaque_auth" structure is an "auth_flavor"
   enumeration followed by bytes which are opaque to (uninterpreted by)
   the RPC protocol implementation.

   The interpretation and semantics of the data contained within the
   authentication fields is specified by individual, independent
   authentication protocol specifications.  (Section 9 defines the
   various authentication protocols.)

   If authentication parameters were rejected, the reply message
   contains information stating why they were rejected.



Sun Microsystems                                                [Page 6]
RFC 1057            Remote Procedure Call, Version 2           June 1988


7.3 Program Number Assignment

   Program numbers are given out in groups of hexadecimal 20000000
   (decimal 536870912) according to the following chart:

                 0 - 1fffffff   defined by Sun
          20000000 - 3fffffff   defined by user
          40000000 - 5fffffff   transient
          60000000 - 7fffffff   reserved
          80000000 - 9fffffff   reserved
          a0000000 - bfffffff   reserved
          c0000000 - dfffffff   reserved
          e0000000 - ffffffff   reserved

   The first group is a range of numbers administered by Sun
   Microsystems and should be identical for all sites.  The second range
   is for applications peculiar to a particular site.  This range is
   intended primarily for debugging new programs.  When a site develops
   an application that might be of general interest, that application
   should be given an assigned number in the first range.  The third
   group is for applications that generate program numbers dynamically.
   The final groups are reserved for future use, and should not be used.

7.4 Other Uses of the RPC Protocol

   The intended use of this protocol is for calling remote procedures.
   Normally, each call message is matched with a reply message.
   However, the protocol itself is a message-passing protocol with which
   other (non-procedure call) protocols can be implemented.  Sun
   currently uses, or perhaps abuses, the RPC message protocol for the
   batching (or pipelining) and broadcast remote procedure calls.

7.4.1 Batching

   Batching is useful when a client wishes to send an arbitrarily large
   sequence of call messages to a server.  Batching typically uses
   reliable byte stream protocols (like TCP) for its transport.  In the
   case of batching, the client never waits for a reply from the server,
   and the server does not send replies to batch calls.  A sequence of
   batch calls is usually terminated by a legitimate remote procedure
   call operation in order to flush the pipeline and get positive
   acknowledgement.

7.4.2 Broadcast Remote Procedure Calls

   In broadcast protocols, the client sends a broadcast call to the
   network and waits for numerous replies.  This requires the use of
   packet-based protocols (like UDP) as its transport protocol.  Servers



Sun Microsystems                                                [Page 7]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   that support broadcast protocols only respond when the call is
   successfully processed, and are silent in the face of errors.
   Broadcast calls use the Port Mapper RPC service to achieve their
   semantics.  See Appendix A for more information.

8. THE RPC MESSAGE PROTOCOL

   This section defines the RPC message protocol in the XDR data
   description language [9].

         enum msg_type {
            CALL  = 0,
            REPLY = 1
         };
   A reply to a call message can take on two forms: The message was
   either accepted or rejected.

         enum reply_stat {
            MSG_ACCEPTED = 0,
            MSG_DENIED   = 1
         };

   Given that a call message was accepted, the following is the status
   of an attempt to call a remote procedure.

         enum accept_stat {
            SUCCESS       = 0, /* RPC executed successfully       */
            PROG_UNAVAIL  = 1, /* remote hasn't exported program  */
            PROG_MISMATCH = 2, /* remote can't support version #  */
            PROC_UNAVAIL  = 3, /* program can't support procedure */
            GARBAGE_ARGS  = 4  /* procedure can't decode params   */
         };

   Reasons why a call message was rejected:

         enum reject_stat {
            RPC_MISMATCH = 0, /* RPC version number != 2          */
            AUTH_ERROR = 1    /* remote can't authenticate caller */
         };












Sun Microsystems                                                [Page 8]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   Why authentication failed:

         enum auth_stat {
            AUTH_BADCRED      = 1,  /* bad credentials (seal broken) */
            AUTH_REJECTEDCRED = 2,  /* client must begin new session */
            AUTH_BADVERF      = 3,  /* bad verifier (seal broken)    */
            AUTH_REJECTEDVERF = 4,  /* verifier expired or replayed  */
            AUTH_TOOWEAK      = 5   /* rejected for security reasons */
         };

   The RPC message:

   All messages start with a transaction identifier, xid, followed by a
   two-armed discriminated union.  The union's discriminant is a
   msg_type which switches to one of the two types of the message.  The
   xid of a REPLY message always matches that of the initiating CALL
   message.  NB: The xid field is only used for clients matching reply
   messages with call messages or for servers detecting retransmissions;
   the service side cannot treat this id as any type of sequence number.

         struct rpc_msg {
            unsigned int xid;
            union switch (msg_type mtype) {
            case CALL:
               call_body cbody;
            case REPLY:
               reply_body rbody;
            } body;
         };

   Body of an RPC call:

   In version 2 of the RPC protocol specification, rpcvers must be equal
   to 2.  The fields prog, vers, and proc specify the remote program,
   its version number, and the procedure within the remote program to be
   called.  After these fields are two authentication parameters:  cred
   (authentication credentials) and verf (authentication verifier).  The
   two authentication parameters are followed by the parameters to the
   remote procedure, which are specified by the specific program
   protocol.











Sun Microsystems                                                [Page 9]
RFC 1057            Remote Procedure Call, Version 2           June 1988


         struct call_body {
            unsigned int rpcvers;       /* must be equal to two (2) */
            unsigned int prog;
            unsigned int vers;
            unsigned int proc;
            opaque_auth cred;
            opaque_auth verf;
            /* procedure specific parameters start here */
         };

   Body of a reply to an RPC call:

         union reply_body switch (reply_stat stat) {
         case MSG_ACCEPTED:
            accepted_reply areply;
         case MSG_DENIED:
            rejected_reply rreply;
         } reply;

   Reply to an RPC call that was accepted by the server:

   There could be an error even though the call was accepted.  The first
   field is an authentication verifier that the server generates in
   order to validate itself to the client.  It is followed by a union
   whose discriminant is an enum accept_stat.  The SUCCESS arm of the
   union is protocol specific.  The PROG_UNAVAIL, PROC_UNAVAIL, and
   GARBAGE_ARGS arms of the union are void.  The PROG_MISMATCH arm
   specifies the lowest and highest version numbers of the remote
   program supported by the server.






















Sun Microsystems                                               [Page 10]
RFC 1057            Remote Procedure Call, Version 2           June 1988


         struct accepted_reply {
            opaque_auth verf;
            union switch (accept_stat stat) {
            case SUCCESS:
               opaque results[0];
               /*
                * procedure-specific results start here
                */
             case PROG_MISMATCH:
                struct {
                   unsigned int low;
                   unsigned int high;
                } mismatch_info;
             default:
                /*
                 * Void.  Cases include PROG_UNAVAIL, PROC_UNAVAIL,
                 * and GARBAGE_ARGS.
                 */
                void;
             } reply_data;
         };

   Reply to an RPC call that was rejected by the server:

   The call can be rejected for two reasons: either the server is not
   running a compatible version of the RPC protocol (RPC_MISMATCH), or
   the server refuses to authenticate the caller (AUTH_ERROR). In case
   of an RPC version mismatch, the server returns the lowest and highest
   supported RPC version numbers.  In case of refused authentication,
   failure status is returned.

         union rejected_reply switch (reject_stat stat) {
         case RPC_MISMATCH:
            struct {
               unsigned int low;
               unsigned int high;
            } mismatch_info;
         case AUTH_ERROR:
            auth_stat stat;
         };






Sun Microsystems                                               [Page 11]
RFC 1057            Remote Procedure Call, Version 2           June 1988


9. AUTHENTICATION PROTOCOLS

   As previously stated, authentication parameters are opaque, but
   open-ended to the rest of the RPC protocol.  This section defines
   some "flavors" of authentication implemented at (and supported by)
   Sun.  Other sites are free to invent new authentication types, with
   the same rules of flavor number assignment as there is for program
   number assignment.

9.1 Null Authentication

   Often calls must be made where the client does not know its identity
   or the server does not care who the client is.  In this case, the
   flavor value (the discriminant of the opaque_auth's union) of the RPC
   message's credentials, verifier, and reply verifier is "AUTH_NULL".
   The bytes of the opaque_auth's body are undefined.  It is recommended
   that the opaque length be zero.

9.2 UNIX Authentication

   The client may wish to identify itself as it is identified on a
   UNIX(tm) system.  The value of the credential's discriminant of an
   RPC call message is "AUTH_UNIX".  The bytes of the credential's
   opaque body encode the the following structure:

         struct auth_unix {
            unsigned int stamp;
            string machinename<255>;
            unsigned int uid;
            unsigned int gid;
            unsigned int gids<16>;
         };

   The "stamp" is an arbitrary ID which the caller machine may generate.
   The "machinename" is the name of the caller's machine (like
   "krypton").  The "uid" is the caller's effective user ID.  The "gid"
   is the caller's effective group ID.  The "gids" is a counted array of
   groups which contain the caller as a member.  The verifier
   accompanying the credentials should be of "AUTH_NULL" (defined
   above).  Note these credentials are only unique within a particular
   domain of machine names, uids, and gids.  Inter-domain naming is
   beyond the scope of this document.

   The value of the discriminant of the reply verifier received in the
   reply message from the server may be "AUTH_NULL" or "AUTH_SHORT".  In
   the case of "AUTH_SHORT", the bytes of the reply verifier's string
   encode an opaque structure.  This new opaque structure may now be
   passed to the server instead of the original "AUTH_UNIX" flavor



Sun Microsystems                                               [Page 12]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   credentials.  The server may keep a cache which maps shorthand opaque
   structures (passed back by way of an "AUTH_SHORT" style reply
   verifier) to the original credentials of the caller.  The caller can
   save network bandwidth and server cpu cycles by using the new
   credentials.

   The server may flush the shorthand opaque structure at any time.  If
   this happens, the remote procedure call message will be rejected due
   to an authentication error.  The reason for the failure will be
   "AUTH_REJECTEDCRED".  At this point, the client may wish to try the
   original "AUTH_UNIX" style of credentials.

9.3 DES Authentication

   UNIX authentication suffers from three major problems:

   (1) The naming is too UNIX oriented.
   (2) There is no universal name, uid, and gid space.
   (3) There is no verifier, so credentials can easily be faked.

   DES authentication attempts to address these problems.

9.3.1 Naming

   The first problem is handled by addressing the client by a simple
   string of characters instead of by an operating system specific
   integer.  This string of characters is known as the "netname" or
   network name of the client. The server is not allowed to interpret
   the contents of the client's name in any other way except to identify
   the client.  Thus, netnames should be unique for every client in the
   Internet.

   It is up to each operating system's implementation of DES
   authentication to generate netnames for its users that insure this
   uniqueness when they call upon remote servers.  Operating systems
   already know how to distinguish users local to their systems. It is
   usually a simple matter to extend this mechanism to the network.  For
   example, a UNIX user at Sun with a user ID of 515 might be assigned
   the following netname: "unix.515@sun.com".  This netname contains
   three items that serve to insure it is unique.  Going backwards,
   there is only one naming domain called "sun.com" in the Internet.
   Within this domain, there is only one UNIX user with user ID 515.
   However, there may be another user on another operating system, for
   example VMS, within the same naming domain that, by coincidence,
   happens to have the same user ID. To insure that these two users can
   be distinguished we add the operating system name. So one user is
   "unix.515@sun.com" and the other is "vms.515@sun.com".




Sun Microsystems                                               [Page 13]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   The first field is actually a naming method rather than an operating
   system name.  It happens that today there is almost a one-to-one
   correspondence between naming methods and operating systems.  If the
   world could agree on a naming standard, the first field could be the
   name of that standard, instead of an operating system name.

9.3.2 DES Authentication Verifiers

   Unlike UNIX authentication, DES authentication does have a verifier
   so the server can validate the client's credential (and vice-versa).
   The contents of this verifier is primarily an encrypted timestamp.
   The server can decrypt this timestamp, and if it is close to the real
   time, then the client must have encrypted it correctly.  The only way
   the client could encrypt it correctly is to know the "conversation
   key" of the RPC session. And if the client knows the conversation
   key, then it must be the real client.

   The conversation key is a DES [5] key which the client generates and
   passes to the server in its first RPC call.  The conversation key is
   encrypted using a public key scheme in this first transaction.  The
   particular public key scheme used in DES authentication is Diffie-
   Hellman [3] with 192-bit keys.  The details of this encryption method
   are described later.

   The client and the server need the same notion of the current time in
   order for all of this to work, perhaps by using the Network Time
   Protocol [4].  If network time synchronization cannot be guaranteed,
   then the client can determine the server's time before beginning the
   conversation using a simpler time request protocol.

   The way a server determines if a client timestamp is valid is
   somewhat complicated. For any other transaction but the first, the
   server just checks for two things:

   (1) the timestamp is greater than the one  previously seen from the
   same client.
   (2) the timestamp has not expired.

   A timestamp is expired if the server's time is later than the sum of
   the client's timestamp plus what is known as the client's "window".
   The "window" is a number the client passes (encrypted) to the server
   in its first transaction.  You can think of it as a lifetime for the
   credential.

   This explains everything but the first transaction.  In the first
   transaction, the server checks only that the timestamp has not
   expired.  If this was all that was done though, then it would be
   quite easy for the client to send random data in place of the



Sun Microsystems                                               [Page 14]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   timestamp with a fairly good chance of succeeding.  As an added
   check, the client sends an encrypted item in the first transaction
   known as the "window verifier" which must be equal to the window
   minus 1, or the server will reject the credential.

   The client too must check the verifier returned from the server to be
   sure it is legitimate.  The server sends back to the client the
   encrypted timestamp it received from the client, minus one second.
   If the client gets anything different than this, it will reject it.

9.3.3 Nicknames and Clock Synchronization

   After the first transaction, the server's DES authentication
   subsystem returns in its verifier to the client an integer "nickname"
   which the client may use in its further transactions instead of
   passing its netname, encrypted DES key and window every time. The
   nickname is most likely an index into a table on the server which
   stores for each client its netname, decrypted DES key and window.

   Though they originally were synchronized, the client's and server's
   clocks can get out of sync again.  When this happens the client RPC
   subsystem most likely will get back "RPC_AUTHERROR" at which point it
   should resynchronize.

   A client may still get the "RPC_AUTHERROR" error even though it is
   synchronized with the server.  The reason is that the server's
   nickname table is a limited size, and it may flush entries whenever
   it wants.  A client should resend its original credential in this
   case and the server will give it a new nickname.  If a server
   crashes, the entire nickname table gets flushed, and all clients will
   have to resend their original credentials.

9.3.4 DES Authentication Protocol Specification

   There are two kinds of credentials: one in which the client uses its
   full network name, and one in which it uses its "nickname" (just an
   unsigned integer) given to it by the server.  The client must use its
   fullname in its first transaction with the server, in which the
   server will return to the client its nickname.  The client may use
   its nickname in all further transactions with the server. There is no
   requirement to use the nickname, but it is wise to use it for
   performance reasons.

      enum authdes_namekind {
         ADN_FULLNAME = 0,
         ADN_NICKNAME = 1
      };




Sun Microsystems                                               [Page 15]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   A 64-bit block of encrypted DES data:

   typedef opaque des_block[8];

   Maximum length of a network user's name:

   const MAXNETNAMELEN = 255;

   A fullname contains the network name of the client, an encrypted
   conversation key and the window. The window is actually a lifetime
   for the credential.  If the time indicated in the verifier timestamp
   plus the window has past, then the server should expire the request
   and not grant it.  To insure that requests are not replayed, the
   server should insist that timestamps are greater than the previous
   one seen, unless it is the first transaction.  In the first
   transaction, the server checks instead that the window verifier is
   one less than the window.

   struct authdes_fullname {
      string name<MAXNETNAMELEN>;  /* name of client                */
      des_block key;               /* PK encrypted conversation key */
      opaque window[4];            /* encrypted window              */
   };

   A credential is either a fullname or a nickname:

   union authdes_cred switch (authdes_namekind adc_namekind) {
   case ADN_FULLNAME:
      authdes_fullname adc_fullname;
   case ADN_NICKNAME:
      int adc_nickname;
   };

   A timestamp encodes the time since midnight,   March 1, 1970.

   struct timestamp {
      unsigned int seconds;    /* seconds          */
      unsigned int useconds;   /* and microseconds */
   };

   Verifier: client variety.

   The window verifier is only used in the first transaction.  In
   conjunction with a fullname credential, these items are packed into
   the following structure before being encrypted:






Sun Microsystems                                               [Page 16]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   struct {
       adv_timestamp;        -- one DES block
       adc_fullname.window;  -- one half DES block
       adv_winverf;          -- one half DES block
   }

   This structure is encrypted using CBC mode encryption with an input
   vector of zero.  All other encryptions of timestamps use ECB mode
   encryption.

   struct authdes_verf_clnt {
      des_block adv_timestamp;    /* encrypted timestamp       */
      opaque adv_winverf[4];      /* encrypted window verifier */
   };

      Verifier: server variety.

   The server returns (encrypted) the same timestamp the client gave it
   minus one second.  It also tells the client its nickname to be used
   in future transactions (unencrypted).

   struct authdes_verf_svr {
      des_block adv_timeverf;     /* encrypted verifier      */
      int adv_nickname;      /* new nickname for client */
   };

9.3.5 Diffie-Hellman Encryption

   In this scheme, there are two constants "BASE" and "MODULUS" [3].
   The particular values Sun has chosen for these for the DES
   authentication protocol are:

   const BASE = 3;
   const MODULUS = "d4a0ba0250b6fd2ec626e7efd637df76c716e22d0944b88b"

   The way this scheme works is best explained by an example.  Suppose
   there are two people "A" and "B" who want to send encrypted messages
   to each other.  So, A and B both generate "secret" keys at random
   which they do not reveal to anyone.  Let these keys be represented as
   SK(A) and SK(B).  They also publish in a public directory their
   "public" keys. These keys are computed as follows:

         PK(A) = ( BASE ** SK(A) ) mod MODULUS
         PK(B) = ( BASE ** SK(B) ) mod MODULUS

   The "**" notation is used here to represent exponentiation. Now, both
   A and B can arrive at the "common" key between them, represented here
   as CK(A, B), without revealing their secret keys.



Sun Microsystems                                               [Page 17]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   A computes:

      CK(A, B) = ( PK(B) ** SK(A)) mod MODULUS

   while B computes:

      CK(A, B) = ( PK(A) ** SK(B)) mod MODULUS

   These two can be shown to be equivalent:

      (PK(B) ** SK(A)) mod MODULUS = (PK(A) ** SK(B)) mod MODULUS

   We drop the "mod MODULUS" parts and assume modulo arithmetic to
   simplify things:

      PK(B) ** SK(A) = PK(A) ** SK(B)

   Then, replace PK(B) by what B computed earlier and likewise for PK(A).

      ((BASE ** SK(B)) ** SK(A) = (BASE ** SK(A)) ** SK(B)

   which leads to:

      BASE ** (SK(A) * SK(B)) = BASE ** (SK(A) * SK(B))

   This common key CK(A, B) is not used to encrypt the timestamps used
   in the protocol. Rather, it is used only to encrypt a conversation
   key which is then used to encrypt the timestamps.  The reason for
   doing this is to use the common key as little as possible, for fear
   that it could be broken.  Breaking the conversation key is a far less
   serious offense, since conversations are relatively short-lived.

   The conversation key is encrypted using 56-bit DES keys, yet the
   common key is 192 bits.  To reduce the number of bits, 56 bits are
   selected from the common key as follows. The middle-most 8-bytes are
   selected from the common key, and then parity is added to the lower
   order bit of each byte, producing a 56-bit key with 8 bits of parity.

10. RECORD MARKING STANDARD

   When RPC messages are passed on top of a byte stream transport
   protocol (like TCP), it is necessary to delimit one message from
   another in order to detect and possibly recover from protocol errors.
   This is called record marking (RM).  Sun uses this RM/TCP/IP
   transport for passing RPC messages on TCP streams.  One RPC message
   fits into one RM record.

   A record is composed of one or more record fragments.  A record



Sun Microsystems                                               [Page 18]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   fragment is a four-byte header followed by 0 to (2**31) - 1 bytes of
   fragment data.  The bytes encode an unsigned binary number; as with
   XDR integers, the byte order is from highest to lowest.  The number
   encodes two values -- a boolean which indicates whether the fragment
   is the last fragment of the record (bit value 1 implies the fragment
   is the last fragment) and a 31-bit unsigned binary value which is the
   length in bytes of the fragment's data.  The boolean value is the
   highest-order bit of the header; the length is the 31 low-order bits.
   (Note that this record specification is NOT in XDR standard form!)

11. THE RPC LANGUAGE

   Just as there was a need to describe the XDR data-types in a formal
   language, there is also need to describe the procedures that operate
   on these XDR data-types in a formal language as well.  The RPC
   Language is an extension to the XDR language, with the addition of
   "program", "procedure", and "version" declarations.  The following
   example is used to describe the essence of the language.

































Sun Microsystems                                               [Page 19]
RFC 1057            Remote Procedure Call, Version 2           June 1988


11.1 An Example Service Described in the RPC Language

   Here is an example of the specification of a simple ping program.

      program PING_PROG {
            /*
             * Latest and greatest version
             */
            version PING_VERS_PINGBACK {
               void
               PINGPROC_NULL(void) = 0;

               /*
                * Ping the client, return the round-trip time
                * (in microseconds). Returns -1 if the operation
                * timed out.
                */
               int
               PINGPROC_PINGBACK(void) = 1;
            } = 2;

            /*
             * Original version
             */
            version PING_VERS_ORIG {
               void
               PINGPROC_NULL(void) = 0;
            } = 1;
         } = 1;

         const PING_VERS = 2;      /* latest version */

   The first version described is PING_VERS_PINGBACK with two
   procedures, PINGPROC_NULL and PINGPROC_PINGBACK.  PINGPROC_NULL takes
   no arguments and returns no results, but it is useful for computing
   round-trip times from the client to the server and back again.  By
   convention, procedure 0 of any RPC protocol should have the same
   semantics, and never require any kind of authentication.  The second
   procedure is used for the client to have the server do a reverse ping
   operation back to the client, and it returns the amount of time (in
   microseconds) that the operation used.  The next version,
   PING_VERS_ORIG, is the original version of the protocol and it does
   not contain PINGPROC_PINGBACK procedure. It is useful for
   compatibility with old client programs, and as this program matures
   it may be dropped from the protocol entirely.






Sun Microsystems                                               [Page 20]
RFC 1057            Remote Procedure Call, Version 2           June 1988


11.2 The RPC Language Specification

   The RPC language is identical to the XDR language defined in RFC
   1014, except for the added definition of a "program-def" described
   below.

   program-def:
      "program" identifier "{"
         version-def
         version-def *
      "}" "=" constant ";"

   version-def:
      "version" identifier "{"
          procedure-def
          procedure-def *
      "}" "=" constant ";"

   procedure-def:
      type-specifier identifier "(" type-specifier
        ("," type-specifier )* ")" "=" constant ";"

11.3 Syntax Notes

   (1) The following keywords are added and cannot be used as
   identifiers: "program" and "version";

   (2) A version name cannot occur more than once within the scope of a
   program definition. Nor can a version number occur more than once
   within the scope of a program definition.

   (3) A procedure name cannot occur more than once within the scope of
   a version definition. Nor can a procedure number occur more than once
   within the scope of version definition.

   (4) Program identifiers are in the same name space as constant and
   type identifiers.

   (5) Only unsigned constants can be assigned to programs, versions and
   procedures.











Sun Microsystems                                               [Page 21]
RFC 1057            Remote Procedure Call, Version 2           June 1988


APPENDIX A: PORT MAPPER PROGRAM PROTOCOL

   The port mapper program maps RPC program and version numbers to
   transport-specific port numbers.  This program makes dynamic binding
   of remote programs possible.

   This is desirable because the range of reserved port numbers is very
   small and the number of potential remote programs is very large.  By
   running only the port mapper on a reserved port, the port numbers of
   other remote programs can be ascertained by querying the port mapper.

   The port mapper also aids in broadcast RPC.  A given RPC program will
   usually have different port number bindings on different machines, so
   there is no way to directly broadcast to all of these programs. The
   port mapper, however, does have a fixed port number.  So, to
   broadcast to a given program, the client actually sends its message
   to the port mapper located at the broadcast address. Each port mapper
   that picks up the broadcast then calls the local service specified by
   the client.  When the port mapper gets the reply from the local
   service, it sends the reply on back to the client.

A.1 Port Mapper Protocol Specification (in RPC Language)

         const PMAP_PORT = 111;      /* portmapper port number */

   A mapping of (program, version, protocol) to port number:

         struct mapping {
            unsigned int prog;
            unsigned int vers;
            unsigned int prot;
            unsigned int port;
         };

   Supported values for the "prot" field:

         const IPPROTO_TCP = 6;      /* protocol number for TCP/IP */
         const IPPROTO_UDP = 17;     /* protocol number for UDP/IP */

   A list of mappings:

         struct *pmaplist {
            mapping map;
            pmaplist next;
         };






Sun Microsystems                                               [Page 22]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   Arguments to callit:

         struct call_args {
            unsigned int prog;
            unsigned int vers;
            unsigned int proc;
            opaque args<>;
         };

   Results of callit:

         struct call_result {
            unsigned int port;
            opaque res<>;
         };

   Port mapper procedures:

         program PMAP_PROG {
            version PMAP_VERS {
               void
               PMAPPROC_NULL(void)         = 0;

               bool
               PMAPPROC_SET(mapping)       = 1;

               bool
               PMAPPROC_UNSET(mapping)     = 2;

               unsigned int
               PMAPPROC_GETPORT(mapping)   = 3;

               pmaplist
               PMAPPROC_DUMP(void)         = 4;

               call_result
               PMAPPROC_CALLIT(call_args)  = 5;
            } = 2;
         } = 100000;

A.2 Port Mapper Operation

   The portmapper program currently supports two protocols (UDP and
   TCP).  The portmapper is contacted by talking to it on assigned port
   number 111 (SUNRPC) on either of these protocols.






Sun Microsystems                                               [Page 23]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   The following is a description of each of the portmapper procedures:

   PMAPPROC_NULL:

   This procedure does no work.  By convention, procedure zero of any
   protocol takes no parameters and returns no results.

   PMAPPROC_SET:

   When a program first becomes available on a machine, it registers
   itself with the port mapper program on the same machine.  The program
   passes its program number "prog", version number "vers", transport
   protocol number "prot", and the port "port" on which it awaits
   service request.  The procedure returns a boolean reply whose value
   is "TRUE" if the procedure successfully established the mapping and
   "FALSE" otherwise.  The procedure refuses to establish a mapping if
   one already exists for the tuple "(prog, vers, prot)".

   PMAPPROC_UNSET:

   When a program becomes unavailable, it should unregister itself with
   the port mapper program on the same machine.  The parameters and
   results have meanings identical to those of "PMAPPROC_SET".  The
   protocol and port number fields of the argument are ignored.

   PMAPPROC_GETPORT:

   Given a program number "prog", version number "vers", and transport
   protocol number "prot", this procedure returns the port number on
   which the program is awaiting call requests.  A port value of zeros
   means the program has not been registered.  The "port" field of the
   argument is ignored.

   PMAPPROC_DUMP:

   This procedure enumerates all entries in the port mapper's database.
   The procedure takes no parameters and returns a list of program,
   version, protocol, and port values.

   PMAPPROC_CALLIT:

   This procedure allows a client to call another remote procedure on
   the same machine without knowing the remote procedure's port number.
   It is intended for supporting broadcasts to arbitrary remote programs
   via the well-known port mapper's port.  The parameters "prog",
   "vers", "proc", and the bytes of "args" are the program number,
   version number, procedure number, and parameters of the remote
   procedure.  Note:



Sun Microsystems                                               [Page 24]
RFC 1057            Remote Procedure Call, Version 2           June 1988


   (1) This procedure only sends a reply if the procedure was
   successfully executed and is silent (no reply) otherwise.

   (2) The port mapper communicates with the remote program using UDP
   only.

   The procedure returns the remote program's port number, and the reply
   is the reply of the remote procedure.

---

# Petición-respuesta
En informática, el esquema, protocolo o modelo de petición-respuesta es una de las principales formas en que las computadoras se comunican entre sí en una red. Consiste en que una computadora envía una petición a otra para solicitar un recurso y la segunda recibe esa petición, la procesa y devuelve un mensaje de respuesta. Este modelo es muy representativo en el paso de mensajes y en el intercambio bidireccional de mensajes tal como se puede observar en la metodología cliente-servidor. En particular, dichos protocolos proporcionan soporte de nivel relativamente bajo para solicitar la ejecución de una operación remota, y también proporcionan soporte directo para RPC (Remote procedure call) y RMI (Remote method invocation).

Esta forma de comunicación está diseñada para apoyar los roles y los intercambios de mensajes en interacciones típicas cliente-servidor.

Normalmente, la comunicación de petición-respuesta es síncrona ya que el proceso cliente se bloquea hasta la llegada de la respuesta del servidor, pudiendo interpretarse como una forma de reconocimiento del cliente por parte del servidor. Aunque en ocasiones, cuando los clientes pueden permitirse el lujo de recuperar la respuesta más tarde, la comunicación puede que sea asíncrona.

Primitivas
El protocolo se basa en un trío de primitivas[1] de comunicación. En el protocolo habrá el mismo número de peticiones que respuestas y puede estar diseñado para proporcionar ciertas garantías de entrega, como si utilizamos datagramas UDP, en cuyo caso el protocolo debe garantizar las entregas mediante acuses de recibo por parte del servidor hacia el cliente.


doOperation(s,args): los clientes utilizan este método para invocar operaciones remotas. Sus argumentos especifican el servidor remoto (s) y que operación invocar junto con información adicionar requerida por la operación (args). Este método envía un mensaje de solicitud al servidor y después invoca receive para obtener el mensaje de respuesta del cual extrae el resultado y lo devuelve al invocador del método. Este invocador permanece bloqueado hasta que el servidor realiza la operación solicitada y devuelve un mensaje de respuesta al cliente.
getRequest: lo utiliza el servidor para obtener las peticiones de servicio.
sendReply(r,c): cuando el servidor ha realizado la operación que se le ha solicitado usa sendReply para enviar el mensaje de respuestas al cliente. Cuando el cliente recibe el mensaje de respuesta se desbloquea y su ejecución puede continuar. Respecto a sus parámetros, el primer campo indica si el mensaje es una solicitud o una respuesta. El segundo campo contiene un identificador de un mensaje.
Identificadores de mensajes
Cualquier esquema que implique la gestión de mensajes para realizar acciones como la entrega segura de mensajes o la solicitud de respuesta requerirá que cada mensaje tenga un identificador de mensaje único mediante el cual pueda ser referenciado.

En este caso, los identificadores de mensajes cuentan con dos partes, una que hace que el identificador sea único para el remitente, y la segundo parte que hace que ese mensaje sea único en el sistema distribuido.

Una operación en el cliente genera el identificador para cada mensaje de solicitud y el servidor copia estos identificadores en los mensajes de respuesta correspondientes para que doOperation pueda verificar que una respuesta es el resultado de una solicitud actual y uno una anterior que esté retrasada.

Modelo de fallo
Por omisión: el servidor no responde o el mensaje se pierde. Para solucionar esto, doOperation puede implementar un timeout en su espera por el mensaje de respuesta. Hay varias opciones sobre lo que puede hacer una vez finalice el tiempo de espera, la más simple es finalizar con un mensaje de que la operación ha fallado. Pero esto no es lo que sucede habitualmente. Lo normal es que para compensar la posibilidad de que haya mensajes perdidos, doOperation envíe el mensaje de solicitud repetidamente hasta que recibe una respuesta o bien determina que el servidor está caído porque han pasado un determinado número de intentos. Finalmente, cuando regresa doOperation, indicará al cliente, a través de una excepción, que no se ha recibido ningún resultado.
Bizantino: el servidor recibe peticiones duplicadas. Hay ocasiones en los que el mensaje de solicitud es retrasmitido y el servidor puede recibirlo más de una vez, como por ejemplo si tarda más en ejecutar la operación que el tiempo de espera del cliente, en cuyo caso este volverá a enviársela. Esto puede llevar a que se realicen determinadas operaciones más de una vez por error. Para evitar esto, el protocolo está diseñado para reconocer mensajes sucesivos del mismo cliente y con el mismo identificador y poder filtrar los duplicados. Si ya la ha realizado y la operación es idempotente, la realiza de nuevo; si no, puede implementar un histórico con los resultados de las operaciones realizadas.
Variaciones
Request (R): el cliente envía un único mensaje de solicitud al servidor. Este protocolo puede ser utilizado cuando no hay ningún valor que devolver de la operación remota y el cliente no requiere de confirmación de que la operación se haya realizado.
Request-Reply (RR): este es el protocolo básico, el cual es útil para la mayoría de los intercambios cliente-servidor. No se requieren mensajes ack, ya que el mensaje de respuesta de un servidor se considera como un reconocimiento de cliente. Del mismo modo, una llamada posterior de un cliente puede considerarse como un acuse de recibo del mensaje de respuesta de un servidor.
Request-Reply-Acknowledge (RRA): el protocolo se basa en el intercambio de tres tipos de mensaje: petición, respuesta y confirmación de respuesta. El mensaje de confirmación de respuesta contiene un identificador del mensaje de respuesta para poder ser reconocido. Esto permitirá al servidor descartar algunos mensajes. La llegada del mensaje de confirmación será interpretado como un acuse de recibo de todos los mensajes de respuesta con un identificador de solicitud más bajo que el de este. Aunque el intercambio involucra un mensaje adicional, no es necesario bloquear al cliente ya que el reconocimiento puede ser transmitido después de que se haya dado la respuesta al cliente. La principal desventaja de este modo es que utiliza más recursos.
Ejemplos
Un buen ejemplo de protocolo de petición-respuesta es HTTP (Hypertext Transfer Protocol) el cual es un protocolo que permite hacer peticiones a servidores web (páginas HTML, Applets, Servlets...)

Las peticiones especifican una URL que incluye:

DNS host servidor
Número de puerto
Identificador de un recurso
Define una serie de métodos de petición:[2]

PUT: reemplaza todas las representaciones actuales del recurso de destino con la carga útil de la petición.
GET: pide el recurso cuya URL se da como argumento. Si es un dato, el servidor lo retorna Si es un programa, el servidor lo ejecuta y devuelve su salida Se puede añadir argumentos a la URL para el programa.
POST: especifica la URL de un recurso que pueda tratar con los datos suministrados en el cuerpo del mensaje (postear en una lista de correo/modificar un perfil web, añadir entradas a una base de datos, proveer datos a un programa (p.ej un formulario)).
UPDATE: actualiza un recurso existente.
DELETE: se utiliza para eliminar un registro existente..

---
Writing Remote Procedural Calls (RPCs) in C

by Jeff Bezanson
Getting to Know RPCs

Perhaps you are bored of writing programs that run on only one computer at a time—how limiting! If so, you'll be glad to hear that there's a tool designed to get your programs on the network in a hurry: remote procedure calls (RPCs). The idea of RPCs is to enable network programming without requiring you to learn new interfaces like BSD sockets. Instead, network communication is done through an interface all programmers are already quite familiar with: the humble function call. The semantics are simple; you make a function call and control transfers to some other code as usual, except the code happens to be running on a different machine.

Hopefully this strikes you as a neat idea, and I'm not sure why you don't hear about RPCs more often. My guess is that programmers don't like to think about the new failure modes that are introduced. All of a sudden, what looks like a simple function call can fail due to any of the myriad issues that plague networks: broken links, long ping times, low bandwidth, malicious users, and so on. Our systems professors try to get it through our skulls that, no matter what, every operation can fail, but shamefully we acknowledge this only about as far as checking for -1 after a getline or recv. If a call is going to fail, the thinking goes, it had better look like one of these. That's my theory anyway. But if you're comfortable checking errors outside your I/O subsystem, read on and reap the benefits of networking without system calls or the obscure data structures they tend to require.

Several different RPC implementations exist; I'll be using the one provided with glibc on Linux. There are also several ways to use RPCs, and I won't be able to go into all the details here. Hopefully I'll be able to get you up and running, and if you want more, O'Reilly has a pretty good book on the subject called Power Programming with RPC. I'm also planning a follow-up article showing how to deal with some of the shortcomings of RPCs by using them asynchronously, so stay tuned.

Let's begin. RPCs could be used for any network application in theory, but are particularly attractive for distributed computation—when you've got a huge amount of scientific data to crunch and systems programmers are in short supply. Like most network applications, a program using RPC can be understood with the client/server model. The client is a program making a call, and the "RPC server" is the receiver that actually performs the function. In other words, an RPC server provides the service of executing some code for you. Simple enough, but before we can actually make an RPC, we have to wonder: how do you pass arguments over a network? Sending a primitive type like an int should be easy enough, but you also might want to pass a pointer to a data structure in memory. Sending the pointer over the network won't do anybody any good: it's an address in your memory after all, which will not be valid on some other machine.

RPC solves this problem for you, using a mechanism called XDR. XDR is able to take your data structure, serialize it behind the scenes, send it over the network, and reassemble it properly on the other end. This service is one of the major benefits of using RPC; not only do you not have to program sockets, you don't have to write tedious and error-prone functions to serialize your linked structures either. In fact, XDR works by reading a description of your data structures and generating such code for you. The program that does this is called an RPC compiler, and its language for describing data structures is similar to C. You describe a procedure call interface along with the data specifications, and the RPC compiler generates client stubs (which you call on the client side to issue an RPC) and server stubs (which will be called on the server side when a request comes in).

Skip this paragraph
Big tangent: XDR is cool because it almost solves an interesting and important open problem in programming practice, which is to create a framework for dynamically sharing structured data, which would, among other things, implement a standard interface for serializing such data. XDR lets you describe data, but unfortunately, this metadata does not persist at run time; you're still stuck with all your code having to assume particular data structures at some point. Technologies like XML, gconf, the GIMP's procedural database, plugins, extension languages, and SQL all sort of chip away at the issue from different angles, but none of them decisively nail it.

.NET and Mono pretty much solve the problem, I admit, but they go a little far, extending interoperability to code as well as data. Every language compiler has to be rewritten (instead of just porting a lightweight API), plus you get this big (and let's face it—evil) thing running under you. All the tools are still ad-hoc; you get the Glade UI builder, which is great for putting together standard GTK+ components, but where are the real creative tools? In other words, I won't be convinced until I see something that's hard to use.

I have my ideas about how it should be done—if you're interested, email me and let's set up a CVS repository :)
Using the RPC Compiler
Since you already know what a function call looks like, I will begin from the perspective of data instead of code. The development process will start with specifying data structures using XDR, then specifying an interface. Then we'll write a server, then the client (which, by the way, could very well be the same program). Our server will perform the highly contrived function of printing out some data stored as a linked list, in order to demonstrate passing somewhat complicated data to an RPC.

Here is the input to the RPC compiler, a file called "llist.x" (.x is the conventional extension for these files):
enum color {ORANGE, PUCE, TURQUOISE};

struct list {
	string data<>;
	int key;
	color col;
	list *next;
};

program PRINTER {
	version PRINTER_V1 {
		int PRINT_LIST(list) = 1;
		int SUM_LIST(list) = 2;
	} = 1;
} = 0x2fffffff;
As you can see, enums and structs can be declared using a syntax very similar to C. One obvious variation is the use of angle brackets ("<>") to denote a variable-length element. You can declare variable-length arrays of any type, which the RPC compiler will interpret as a pointer together with an integer length specifier. Strings are null-terminated, though, so no length field is generated in this case. Next we assign an arbitrary ID (0x2fffffff) and version number (1) to our program, and indicate that our program has two RPCs, each with its own unique ID (actually, there is a third RPC, since the RPC compiler automatically generates a "null" RPC with ID #0 for you, to be used for testing). The program ID provides a way to address your application specifically on a system, hiding the details of assigning IP ports. In short, you will be able to specify a server connection simply using a hostname and a program ID. RPC also lets you assign a version number, to prevent clients from attempting to communicate with a different, and possibly incompatible, version of the server they talk to.
Invoke the RPC compiler using "rpcgen llist.x". rpcgen spits a slew of files back out at you:

llist.h: Actual declarations in C, based on your .x file.
llist_svc.c: Implements main() for the server. Handles listening for incoming RPCs and dispatching control appropriately.
llist_xdr.c: Code to convert your data structures for network transmission.
llist_clnt.c: Functions the client can call to issue RPCs.
You should definitely read llist.h, as it contains definitions that every file in your project will need to access. Understanding the other files is less important, but it should make sense to you that llist_xdr.c will be linked with both the client and server, llist_svc.c will be linked with the server, and llist_clnt.c will be linked with the client. The client and server have a certain duality: for the client, rpcgen writes functions for making RPCs and you have to supply main(), whereas for the server, rpcgen writes main() for you and you have to write functions to service incoming RPCs. Therefore we'll be creating two more .c files: llist_svc_proc.c (implementing RPCs on the server side) and llist.c (the client).
Looking at the bottom of llist.h, we find function prototypes:

#define PRINT_LIST 1
extern  int * print_list_1(list *, CLIENT *);
extern  int * print_list_1_svc(list *, struct svc_req *);
#define SUM_LIST 2
extern  int * sum_list_1(list *, CLIENT *);
extern  int * sum_list_1_svc(list *, struct svc_req *);
Names ending in "_svc" are prototypes for the server functions we need to fill in. The other function in each pair is the client stub, already written for us. "CLIENT" is a data type specifying a connection to an RPC server, and "svc_req" gives a server function some information about an incoming request.

---

MQTT
MQTT es un protocolo de red máquina a máquina de publicación/suscripción (publish/subscribe) diseñado para ser ligero. Su propósito es facilitar la conectividad en escenarios con dispositivos de recursos limitados o con ancho de banda restringido, como los habituales en el Internet de las cosas (IoT). Se ejecuta sobre un protocolo de transporte que proporcione conexiones ordenadas, sin pérdidas y bidireccionales, típicamente TCP/IP. Es un estándar abierto de OASIS y una recomendación de la ISO (ISO/IEC 20922).[1][2]

Historia
Andy Stanford-Clark (IBM) y Arlen Nipper (entonces en Eurotech, Inc.) escribieron la primera versión del protocolo en 1999. Su uso original fue la monitorización de oleoductos en un sistema SCADA industrial. El objetivo era disponer de un protocolo eficiente en ancho de banda, ligero y de bajo consumo de batería, dado que los dispositivos se conectaban por enlace satelital, muy costoso en aquella época.[3]

Históricamente, el «MQ» de «MQTT» procedía de la línea de productos IBM MQ (entonces denominada «MQSeries»), donde la sigla representaba Message Queue («cola de mensajes»). El protocolo, sin embargo, proporciona mensajería de publicación/suscripción y no maneja realmente colas, pese a su nombre. En la especificación liberada por IBM como versión 3.1, el protocolo se denominaba «MQ Telemetry Transport». Las versiones publicadas posteriormente por OASIS se refieren ya simplemente como «MQTT», aunque el comité técnico responsable conserva la denominación OASIS Message Queuing Telemetry Transport Technical Committee.[4] Desde 2013, «MQTT» no es ya un acrónimo de nada.[4]

En 2013, IBM cedió MQTT v3.1 al organismo de normalización OASIS bajo una carta de comité que solo permitía cambios menores sobre la especificación. Tras asumir el mantenimiento del estándar, OASIS publicó la versión 3.1.1 el 29 de octubre de 2014.[5] En 2016, esa misma versión fue aprobada como estándar internacional ISO/IEC 20922:2016.[1] Una actualización más sustancial, MQTT versión 5, que añadió numerosas características nuevas, se publicó el 7 de marzo de 2019.[2]

Especificaciones derivadas
MQTT-SN (MQTT for Sensor Networks) es una variante del protocolo principal orientada a dispositivos embebidos alimentados por batería que operan sobre redes no TCP/IP. Puede emplearse sobre cualquier red que ofrezca transporte bidireccional —como el UDP o los enlaces serie—, aunque su objetivo de diseño original fue Zigbee. La comunicación dentro de una red MQTT-SN, así como su interconexión con la red MQTT externa, se coordina mediante una pasarela MQTT-SN.[6]

---

¿Qué es MQTT?
MQTT es un protocolo de mensajería basado en estándares, o un conjunto de reglas, que se utiliza para la comunicación de un equipo a otro. Los sensores inteligentes, los dispositivos portátiles y otros dispositivos de Internet de las cosas (IoT) generalmente tienen que transmitir y recibir datos a través de una red con recursos restringidos y un ancho de banda limitado. Estos dispositivos IoT utilizan MQTT para la transmisión de datos, ya que resulta fácil de implementar y puede comunicar datos IoT de manera eficiente. MQTT admite la mensajería entre dispositivos a la nube y la nube al dispositivo.

¿Por qué es importante el protocolo MQTT?
El protocolo MQTT se ha convertido en un estándar para la transmisión de datos de IoT, ya que ofrece los siguientes beneficios:

Ligero y eficiente
La implementación de MQTT en el dispositivo IoT requiere recursos mínimos, por lo que se puede usar incluso en pequeños microcontroladores. Por ejemplo, un mensaje de control MQTT mínimo puede tener tan solo dos bytes de datos. Los encabezados de los mensajes MQTT también son pequeños para poder optimizar el ancho de banda de la red.

Escalable
La implementación de MQTT requiere una cantidad mínima de código que consume muy poca energía en las operaciones. El protocolo también tiene funciones integradas para admitir la comunicación con una gran cantidad de dispositivos IoT. Por tanto, puede implementar el protocolo MQTT para conectarse con millones de estos dispositivos.

Fiable
Muchos dispositivos IoT se conectan a través de redes celulares poco fiables con bajo ancho de banda y alta latencia. MQTT tiene funciones integradas que reducen el tiempo que tarda el dispositivo IoT en volver a conectarse con la nube. También define tres niveles diferentes de calidad de servicio a fin de garantizar la fiabilidad para los casos de uso de IoT: como máximo una vez (0), al menos una vez (1) y exactamente una vez (2).

Seguro
MQTT facilita a los desarrolladores el cifrado de mensajes y la autenticación de dispositivos y usuarios mediante protocolos de autenticación modernos, como OAuth, TLS1.3, certificados administrados por el cliente, etc.

Admitido
Varios lenguajes, como Python, tienen un amplio soporte para la implementación del protocolo MQTT. Por lo tanto, los desarrolladores pueden implementarlo rápidamente con una codificación mínima en cualquier tipo de aplicación.

¿Qué historia hay detrás del protocolo MQTT?
El protocolo MQTT se inventó en 1999 para su uso en la industria del petróleo y el gas. Los ingenieros necesitaban un protocolo para un ancho de banda mínimo y una pérdida de batería mínima para supervisar los oleoductos vía satélite. Inicialmente, el protocolo se conocía como transporte de telemetría de Message Queue Server debido al producto de IBM MQ Series que admitió por primera vez su fase inicial. En 2010, IBM lanzó MQTT 3.1 como un protocolo gratuito y abierto para que cualquiera pudiera implementarlo, que después, en 2013, se envió al organismo de especificación de la Organización para el Avance de Estándares de Información Estructurada (OASIS) para su mantenimiento. En 2019, OASIS lanzó una versión 5 de MQTT actualizada. Ahora MQTT ya no es un acrónimo sino que se considera el nombre oficial del protocolo. 

¿Cuál es el principio en el que se basa MQTT?
El protocolo MQTT funciona según los principios del modelo de publicación o suscripción. En la comunicación de red tradicional, los clientes y servidores se comunican directamente entre sí. Los clientes solicitan recursos o datos del servidor, a continuación el servidor procesa y envía una respuesta. Sin embargo, MQTT utiliza un patrón de publicación o suscripción para desacoplar el remitente del mensaje (editor) del receptor del mensaje (suscriptor). En lugar de ello, un tercer componente, denominado agente de mensajes, controla la comunicación entre editores y suscriptores. El trabajo del agente consiste en filtrar todos los mensajes entrantes de los editores y distribuirlos correctamente a los suscriptores. El agente desacopla los editores y suscriptores de la siguiente manera:

Desacoplamiento espacial
El editor y el suscriptor no conocen la ubicación de la red del otro y no intercambian información como direcciones IP o números de puerto.

Desacoplamiento de tiempo
El editor y el suscriptor no se ejecutan ni tienen conectividad de red al mismo tiempo.

Desacoplamiento de sincronización
Tanto los editores como los suscriptores pueden enviar o recibir mensajes sin interrumpirse entre sí. Por ejemplo, el suscriptor no tiene que esperar a que el editor envíe un mensaje.

¿Qué son los componentes MQTT?
MQTT implementa el modelo de publicación o suscripción mediante la definición de clientes y agentes, tal y como se muestra a continuación.

Cliente MQTT
Un cliente MQTT es cualquier dispositivo, desde un servidor hasta un microcontrolador, que ejecuta una biblioteca MQTT. Si el cliente envía mensajes, actúa como editor, y si recibe mensajes, actúa como receptor. Básicamente, cualquier dispositivo que se comunique mediante MQTT a través de una red puede denominarse dispositivo cliente MQTT.

Agente MQTT
El agente MQTT es el sistema de back-end que coordina los mensajes entre los diferentes clientes. Las responsabilidades del agente incluyen recibir y filtrar mensajes, identificar a los clientes suscritos a cada mensaje y enviarles los mensajes. También se encarga de otras tareas como:

La autorización y autenticación de clientes MQTT
Pasar mensajes a otros sistemas para su posterior análisis
El control de mensajes perdidos y sesiones de clientes
Conexión MQTT
Los clientes y los agentes comienzan a comunicarse mediante una conexión MQTT. Los clientes inician la conexión al enviar un mensaje CONECTAR al agente MQTT. El agente confirma que se ha establecido una conexión al responder con un mensaje CONNACK. Tanto el cliente MQTT como el agente requieren una pila TCP o IP para comunicarse. Los clientes nunca se conectan entre sí, solo con el agente.

¿Cómo funciona MQTT?
A continuación se proporciona una descripción general del funcionamiento de MQTT. 

Un cliente MQTT establecer una conexión con el agente MQTT.
Una vez conectado, el cliente puede publicar mensajes, suscribirse a mensajes específicos o hacer ambas cosas.
Cuando el agente MQTT recibe un mensaje, lo reenvía a los suscriptores que están interesados.
Analicemos los detalles para una mayor comprensión.

Tema de MQTT
El término “tema” se refiere a las palabras clave que utiliza el agente MQTT a fin de filtrar mensajes para los clientes de MQTT. Los temas están organizados jerárquicamente, de forma similar a un directorio de archivos o carpetas. Por ejemplo, considere un sistema doméstico inteligente que opera en una casa de varios pisos que tiene diferentes dispositivos inteligentes en cada uno de ellos. En ese caso, es posible que el agente MQTT organice temas como:

ourhome/groundfloor/livingroom/light

ourhome/firstfloor/kitchen/temperature

Publicación MQTT
Los clientes MQTT publican mensajes que contienen el tema y los datos en formato de bytes. El cliente determina el formato de los datos, como datos de texto, datos binarios, archivos XML o JSON. Por ejemplo, es posible que una lámpara del sistema doméstico inteligente publique un mensaje sobre el tema salón o luz.

Suscripción MQTT
Los clientes MQTT envían un mensaje SUBSCRIBE (SUBSCRIBIRSE) al agente MQTT para recibir mensajes sobre temas de interés. Este mensaje contiene un identificador único y una lista de suscripciones. Por ejemplo, la aplicación de hogar inteligente en su teléfono quiere mostrar cuántas luces están encendidas en casa. Se suscribirá a la luz del tema y aumentará el contador para todos los mensajes activados. 

¿Qué es MQTT sobre WSS?
MQTT sobre WebSockets (WSS) es una implementación de MQTT para recibir datos directamente en un navegador web. El protocolo MQTT define un cliente de JavaScript para proporcionar compatibilidad con WSS para navegadores. En este caso, el protocolo funciona como de costumbre, pero agrega encabezados adicionales a los mensajes MQTT para admitir también el protocolo WSS. Puede considerarlo como la carga útil del mensaje MQTT ajustada en un sobre WSS.

¿MQTT es seguro?
La comunicación MQTT utiliza el protocolo SSL para proteger los datos confidenciales que transmiten los dispositivos IoT. Puede implementar la identidad, la autenticación y la autorización entre los clientes y el agente mediante certificados SSL y contraseñas. El agente MQTT normalmente autentica a los clientes mediante sus contraseñas, así como los identificadores de cliente únicos que asigna a cada cliente. En la mayoría de las implementaciones, el cliente autentica el servidor con certificados o búsquedas de DNS. También puede implementar protocolos de cifrado con MQTT. 

¿MQTT es RESTful?
MQTT no es RESTful. La transferencia de estado representacional (REST) es un enfoque arquitectónico para la comunicación de red que utiliza el patrón de comunicación de solicitud-respuesta entre los emisores y receptores de mensajes. Por el contrario, MQTT utiliza el modelo de comunicación de publicación o suscripción en la capa de aplicación y requiere una conexión TCP permanente para transmitir mensajes de manera push. Sin embargo, la versión 5 de MQTT añade un nuevo método de solicitud o respuesta para actuar de manera similar a REST, en el que el editor puede adjuntar un tema de respuesta especial, que el receptor procesa y genera una respuesta adecuada.

¿De qué manera puede AWS admitir la implementación de MQTT?
AWS IoT Core es un servicio totalmente administrado que le permite conectar miles de millones de dispositivos de IoT y enviar billones de mensajes a los servicios de AWS. Puede:

Conectar, administrar y escalar las flotas de dispositivos con facilidad y de manera fiable sin necesidad de aprovisionar ni administrar servidores.
Elija el protocolo de comunicación que prefiera, incluidos MQTT, HTTPS, MQTT a través de WSS y LoRaWAN.
Proteja las conexiones y los datos de los dispositivos con autenticación mutua y cifrado de extremo a extremo.
Filtre y transforme los datos de los dispositivos y actúe en función de estos sobre la marcha, en función de las reglas empresariales definidas.

---

RPC over MQTT
Github Repo	C Header	C source	JS source
mongoose-os-libs/rpc-mqtt	mgos_rpc_channel_mqtt.h	 	 
Overview#
MQTT RPC channel allows invoking RPC calls via MQTT.

Topics#
When rpc.mqtt.enable is true (default), device subscribes to rpc.mqtt.sub_topic (default: ${device.id}/rpc, e.g. esp8266_DA7E15/rpc in the example).

Responses are published to ${src}/rpc, where ${src} is taken from the request's frame, so requests for which a response is expected must include src.

Examples#
Request#
Published to esp8266_DA7E15/rpc:

{ "id": 123, "src": "foo", "method": "Sys.GetInfo"}
Response will be published to foo/rpc.

mos tool support#
mos tool supports RPC over MQTT via the mqtt port schema:

$ mos --port mqtt://iot.eclipse.org/esp8266_DA7E15 call Sys.GetInfo
Bash
For brokers that require TLS, mqtts should be used:

$ mos --port mqtts://iot.eclipse.org/esp8266_DA7E15 call Sys.GetInfo
Bash
For brokers that require TLS client authentication (e.g. AWS), cert and file should be supplied:

$ mos --port mqtts://XXXXXXXX.iot.REGION.amazonaws.com/esp8266_DA7E15 --cert-file mycert.pem --key-file mykey.pem call Sys.GetInfo
mg_rpc_channel_mqtt#
struct mg_rpc_channel *mg_rpc_channel_mqtt(const struct mg_str device_id);
C
__cplusplus

---
RPC over UART
Github Repo	C Header	C source	JS source
mongoose-os-libs/rpc-uart	mgos_rpc_channel_uart.h	 	 
mg_rpc_channel_uart#
struct mg_rpc_channel *mg_rpc_channel_uart(
    const struct mgos_config_rpc_uart *cfg,
    const struct mgos_uart_config *ucfg);
C
__cplusplus

---
RPC over UDP
Github Repo	C Header	C source	JS source
mongoose-os-libs/rpc-udp	mgos_rpc_channel_udp.h	 	 
Overview#
Frames are sent as datagrams. Fragmentation is not supported, so 1 frame = 1 datagram.

Responses are supported, but because UDP is not reliable, it is best suited for notifications.

Listener configuration#
Set rpc.udp.listen_addr to udp://0.0.0.0:1234 to listen on port 1234.

Outbound channel configuration#
To use UDP for outbound RPC, set rpc.udp.dst_addr to the address of the server / peer, e.g. udp://192.168.1.23:1234.

mos tool support#
mos supports UDP transport via the udp:// port scheme, e.g.:

 $ mos call --port udp://192.168.11.5:1234/ Sys.GetInfo