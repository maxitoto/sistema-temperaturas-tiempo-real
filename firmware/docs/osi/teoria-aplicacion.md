An application layer is an abstraction layer that specifies the shared communication protocols and interface methods used by hosts in a communications network.[1]

An application layer abstraction is specified in both the Internet Protocol Suite (TCP/IP) and the OSI model.[2] Although both models use the same term for their respective highest-level layer, the detailed definitions and purposes are different.[3]

Historical and conceptual perspective
icon
This section does not cite any sources. Please help improve this section by adding citations to reliable sources. Unsourced material may be challenged and removed. (October 2025) (Learn how and when to remove this message)
The concept of the application layer emerged from early efforts to standardize network communication by defining distinct layers of protocol functionality. In the OSI model developed in the late 1970s and early 1980s, the application layer was explicitly separated from lower layers like session and presentation to modularize network services and applications for interoperability and clarity. Contrasting this, the TCP/IP model, whose origins date back to the 1970s and which became the foundation of the modern Internet, integrates these functions into a broader application layer that handles process-to-process communication. This design emphasizes simplicity and robustness by placing intelligence and complexity on the network's edge rather than the core, enabling nearly any network to interconnect. As a result, the application layer serves as the interface for diverse networking protocols that drive everyday Internet-based services, reflecting decades of evolution aimed at flexible, interoperable communication across heterogeneous networks.[1]

Internet protocol suite
Internet protocol suite
Application layer
BGPDHCP (v6)DNSFTPHTTP (HTTP/3)HTTPSIMAPJMAPIPPIRCLDAPMGCPMQTTNNTPNTPOSPFPOPPTPONC/RPCRTPRTSPRIPSIPSMTPSNMPSSHTelnetTLS/SSLXMPPmore...
Transport layer
TCPUDPDCCPSCTPRSVPQUICmore...
Internet layer
IP v4v6ICMP (v6)NDPECNL4SIGMPIPsecmore...
Link layer
ARPTunnelsPPPMACmore...
vte
In the Internet protocol suite, the application layer contains the communications protocols and interface methods used in process-to-process communications across an Internet Protocol (IP) computer network.[3] The application layer only standardizes communication and depends upon the underlying transport layer protocols to establish host-to-host data transfer channels and manage the data exchange in a client–server or peer-to-peer networking model.[4] Though the TCP/IP application layer does not describe specific rules or data formats that applications must consider when communicating, the original specification (in RFC 1123) does rely on and recommend the robustness principle for application design.[5][6]

OSI model
OSI model
by layer
7.  Application layer
6.  Presentation layer
5.  Session layer
4.  Transport layer
3.  Network layer
2.  Data link layer
1.  Physical layer
vte
In the OSI model, the definition of the application layer is narrower in scope.[8] The OSI model defines the application layer as only the interface responsible for communicating with host-based and user-facing applications.[9] OSI then explicitly distinguishes the functionality of two additional layers, the session layer and presentation layer, as separate levels below the application layer and above the transport layer. OSI specifies a strict modular separation of functionality at these layers and provides protocol implementations for each. In contrast, the Internet Protocol Suite compiles these functions into a single layer.[9]

Sublayers
Originally, the OSI model consisted of two kinds of application layer services with their related protocols.[10] These two sublayers are the common application service element (CASE) and specific application service element (SASE).[11] Generally, an application layer protocol is realized by using the functionality of several application service elements.[12] Some application service elements invoke different procedures based on the version of the session service available.[13]

CASE
The common application service element sublayer provides services for the application layer and requests services from the session layer. It provides support for common application services, such as:

ACSE (Association Control Service Element)[11]
ROSE (Remote Operation Service Element)
CCR (Commitment Concurrency and Recovery)
RTSE (Reliable Transfer Service Element)
SASE
The specific application service element sublayer provides application-specific services (protocols), such as:

FTAM (File Transfer, Access and Manager)
VT (Virtual Terminal)
MOTIS (Message Oriented Text Interchange Standard)
CMIP (Common Management Information Protocol)
JTM (Job Transfer and Manipulation)[14]
MMS (Manufacturing Messaging Specification)
RDA (Remote Database Access)
DTP (Distributed Transaction Processing)
Protocols
Main category: Application layer protocols
The IETF definition document for the application layer in the Internet Protocol Suite is RFC 1123. It provided an initial set of protocols that covered the major aspects of the functionality of the early Internet:[5]

Hypertext documents: Hypertext Transfer Protocol (HTTP)
Remote login to hosts: Telnet, Secure Shell
File transfer: File Transfer Protocol (FTP), Trivial File Transfer Protocol (TFTP)
Electronic mail transport: Simple Mail Transfer Protocol (SMTP)
Networking support: Domain Name System (DNS)
Host initialization: BOOTP
Remote host management: Simple Network Management Protocol (SNMP), Common Management Information Protocol over TCP (CMOT)
Examples
Additional notable application-layer protocols include the following:

9P, Plan 9 from Bell Labs distributed file system protocol
AFP, Apple Filing Protocol
APPC, Advanced Program-to-Program Communication
AMQP, Advanced Message Queuing Protocol
Atom Publishing Protocol
BEEP, Block Extensible Exchange Protocol
Bitcoin
BitTorrent
CFDP, Coherent File Distribution Protocol
CoAP, Constrained Application Protocol
DDS, Data Distribution Service
DeviceNet
DNS, Domain Name Services
eDonkey
ENRP, Endpoint Handlespace Redundancy Protocol
FastTrack (KaZaa, Grokster, iMesh)
Finger, User Information Protocol
Freenet
FTAM, File Transfer Access and Management
FTP, File Transfer Protocol
Gemini, Gemini protocol
Gopher, Gopher protocol
HL7, Health Level Seven
HTTP, Hypertext Transfer Protocol
Hypercore, formerly dat://
H.323, Packet-Based Multimedia Communications System
IMAP, Internet Message Access Protocol
IRC, Internet Relay Chat
IPFS, InterPlanetary File System
Kademlia
LDAP, Lightweight Directory Access Protocol
LPD, Line Printer Daemon Protocol
MIME (S-MIME), Multipurpose Internet Mail Extensions and Secure MIME
Modbus
MQTT Protocol
Netconf
NFS, Network File System
NIS, Network Information Service
NNTP, Network News Transfer Protocol
NTCIP, National Transportation Communications for Intelligent Transportation System Protocol
NTP, Network Time Protocol
OSCAR, AOL Instant Messenger Protocol
POP, Post Office Protocol
PNRP, Peer Name Resolution Protocol
RDP, Remote Desktop Protocol
RELP, Reliable Event Logging Protocol
RFB, Remote Framebuffer Protocol
Rlogin, Remote Login in UNIX Systems
RPC, Remote Procedure Call
RTMP, Real Time Messaging Protocol
RTP, Real-time Transport Protocol
RTPS, Real Time Publish Subscribe
RTSP, Real Time Streaming Protocol
SAP, Session Announcement Protocol
SDP, Session Description Protocol
SIP, Session Initiation Protocol
SLP, Service Location Protocol
SMB, Server Message Block
SMTP, Simple Mail Transfer Protocol
SNTP, Simple Network Time Protocol
SSH, Secure Shell
SSMS, Secure SMS Messaging Protocol
TCAP, Transaction Capabilities Application Part
TDS, Tabular Data Stream
Tor (anonymity network)
Tox
TSP, Time Stamp Protocol
VTP, Virtual Terminal Protocol
Whois (and RWhois), Remote Directory Access Protocol
WebDAV
WebRTC
WebSocket
X.400, Message Handling Service Protocol
X.500, Directory Access Protocol (DAP)
XMPP, Extensible Messaging and Presence Protocol
Z39.50

---

Real-Time Messaging Protocol

Article
Talk
Read
Edit
View history

Appearance hide
Text

Small

Standard

Large
Width

Standard

Wide
Color

Automatic

Light

Dark
From Wikipedia, the free encyclopedia
(Redirected from Real Time Messaging Protocol)
Real-Time Messaging Protocol
Communication protocol
Abbreviation	RTMP
Purpose	Streaming
Developer	Macromedia
Introduction	
Based on	TCP
OSI layer	Application layer (7)
Port	tcp/1935
Real-Time Messaging Protocol (RTMP) is a communication protocol for streaming audio, video, and data over the Internet. Originally developed as a proprietary protocol by Macromedia for streaming between Flash Player and the Flash Communication Server, Adobe (which acquired Macromedia) has released an incomplete version of the specification of the protocol for public use.

The RTMP protocol has multiple variations:

RTMP proper, the "plain" protocol which works on top of Transmission Control Protocol (TCP) and uses port number 1935 by default.
RTMPS, which is RTMP over a Transport Layer Security (TLS/SSL) connection.
RTMPE, which is RTMP encrypted using Adobe's own security mechanism. While the details of the implementation are proprietary, the mechanism uses industry standard cryptographic primitives.[1]
RTMPT, which is encapsulated within HTTP requests to traverse firewalls. RTMPT is frequently found utilizing cleartext requests on TCP ports 80 and 443 to bypass most corporate traffic filtering. The encapsulated session may carry plain RTMP, RTMPS, or RTMPE packets within.
RTMFP, which is RTMP over User Datagram Protocol (UDP) instead of TCP, replacing RTMP Chunk Stream. The Secure Real-Time Media Flow Protocol suite has been developed by Adobe Systems and enables end‐users to connect and communicate directly with each other (P2P).
E-RTMP, or Enhanced RTMP, is an enhancement to the RTMP and FLV specifications designed to improve streaming capabilities while maintaining compatibility with existing RTMP infrastructure.[2] E-RTMP enhances RTMP by adding features such as advanced timestamp precision, multitrack capabilities, expanded codec support, FourCC signaling, and a reconnect request feature.
While the primary motivation for RTMP was to be a protocol for playing Flash Video, it is also used in some other applications, such as the Adobe LiveCycle Data Services ES.

Basic operation
RTMP is a TCP-based protocol which maintains persistent connections and allows low-latency communication. To deliver streams smoothly and transmit as much information as possible, it splits streams into fragments, and their size is negotiated dynamically between the client and server. Sometimes, it is kept unchanged; the default fragment sizes are 64 bytes for audio data, and 128 bytes for video data and most other data types. Fragments from different streams may then be interleaved, and multiplexed over a single connection. With longer data chunks, the protocol thus carries only a one-byte header per fragment, so incurring very little overhead. However, in practice, individual fragments are not typically interleaved. Instead, the interleaving and multiplexing is done at the packet level, with RTMP packets across several different active channels being interleaved in such a way as to ensure that each channel meets its bandwidth, latency, and other quality-of-service requirements. Packets interleaved in this fashion are treated as indivisible, and are not interleaved on the fragment level.

The RTMP defines several virtual channels on which packets may be sent and received, and which operate independently of each other. For example, there is a channel for handling RPC requests and responses, a channel for video stream data, a channel for audio stream data, a channel for out-of-band control messages (fragment size negotiation, etc.), and so on. During a typical RTMP session, several channels may be active simultaneously at any given time. When RTMP data is encoded, a packet header is generated. The packet header specifies, amongst other matters, the ID of the channel on which it is to be sent, a timestamp of when it was generated (if necessary), and the size of the packet's payload. This header is then followed by the actual payload content of the packet, which is fragmented according to the currently agreed-upon fragment size before it is sent over the connection. The packet header itself is never fragmented, and its size does not count towards the data in the packet's first fragment. In other words, only the actual packet payload (the media data) is subject to fragmentation.

At a higher level, the RTMP encapsulates MP3 or AAC audio and FLV1 video multimedia streams, and can make remote procedure calls (RPCs) using the Action Message Format. Any RPC services required are made asynchronously, using a single client/server request/response model, such that real-time communication is not required.[clarification needed][3][4]

Encryption
RTMP sessions may be encrypted using either of two methods:

Using industry standard TLS/SSL mechanisms. The underlying RTMP session is simply wrapped inside a normal TLS/SSL session.
Using RTMPE, which wraps the RTMP session in a lighter-weight encryption layer.
HTTP tunneling
In RTMP Tunneled (RTMPT), RTMP data is encapsulated and exchanged via HTTP, and messages from the client (the media player, in this case) are addressed to port 80 (the default for HTTP) on the server.

While the messages in RTMPT are larger than the equivalent non-tunneled RTMP messages due to HTTP headers, RTMPT may facilitate the use of RTMP in scenarios where the use of non-tunneled RTMP would otherwise not be possible, such as when the client is behind a firewall that blocks non-HTTP and non-HTTPS outbound traffic.

The protocol works by sending commands through the POST URL, and AMF messages through the POST body. An example is

POST /open/1 HTTP/1.1
for a connection to be opened.

Specification document and patent license
Adobe has released a specification for version 1.0 of the protocol, dated 21 December 2012.[5] The web landing page leading to that specification notes that "To benefit customers who want to protect their content, the open RTMP specification does not include Adobe's unique secure RTMP measures".[6]

A document accompanying the Adobe specification grants "non-exclusive, royalty-free, nontransferable, non-sublicensable, personal, worldwide" patent license to all implementations of the protocol, with two restrictions: one forbids use for intercepting streaming data ("any technology that intercepts streaming video, audio and/or data content for storage in any device or medium"), and another prohibits circumvention of "technological measures for the protection of audio, video and/or data content, including any of Adobe’s secure RTMP measures".[7]

Patents and related litigation
Stefan Richter, author of some books on Flash, noted in 2008 that while Adobe is vague as to which patents apply to RTMP, U.S. patent 7,246,356 appears to be one of them.[3]

In 2011, Adobe did sue Wowza Media Systems claiming, among other things, infringement of their RTMP patents.[8][9][10] In 2015, Adobe and Wowza announced that the lawsuits have been settled and dismissed with prejudice.[11]

Packet structure

RTMP Packet Diagram
Packets are sent over a TCP connection, which is established first between client and server. They contain a header and a body which, in the case of connection and control commands, is encoded using the Action Message Format (AMF). The header is split into the Basic Header (shown as detached from the rest, in the diagram) and Chunk Message Header. The Basic Header is the only constant part of the packet and is usually composed of a single composite byte, where the two most significant bits are the Chunk Type (fmt in the specification) and the rest form the Stream ID. Depending on the value of the former, some fields of the Message Header can be omitted, and their value derived from previous packets while depending on the value of the latter, the Basic Header can be extended with one or two extra bytes (as in the case of the diagramme that has three bytes in total (c)). If the value of the remaining six bits of the Basic Header (BH) (least significant) is 0 then the BH is two bytes and represents from Stream ID 64 to 319 (64+255); if the value is 1, then the BH is three bytes (with last two bytes encoded as 16bit Little Endian) and represents from Stream ID 64 to 65599 (64+65535); if the value is 2, then BH is one byte and is reserved for low-level protocol control messages and commands. The Chunk Message Header contains meta-data information such as the message size (measured in bytes), the Timestamp Delta and Message Type. This last value is a single byte and defines whether the packet is an audio, video, command or "low level" RTMP packet such as an RTMP Ping.

An example is shown below as captured when a flash client executes the following code:

var stream:NetStream = new NetStream(connectionObject);
this will generate the following Chunk:

Hex Code	ASCII
03 00 0B 68 00 00 19 14 00 00 00 00 02 00 0C 63 72 65 61 74 65 53 74 72 65 61 6D 00 40 00 00 00 00 00 00 00 05	␃ ␀ @ I ␀ ␀ ␙ ␔ ␀ ␀ ␀ ␀ ␂ ␀ ␌ c r e a t e S t r e a m ␀ @ ␀ ␀ ␀ ␀ ␀ ␀ ␀ ␅
The packet starts with a Basic Header of a single byte (0x03) where the two most significant bits (b00000011) define a chunk header type of 0 while the rest (b00000011) define a Chunk Stream ID of 3. The four possible values of the header type and their significance are:

b00 = 12 byte header (full header).
b01 = 8 bytes - like type b00, not including message ID (4 last bytes).
b10 = 4 bytes - Basic Header and timestamp (3 bytes) are included.
b11 = 1 byte - only the Basic Header is included.
The last type (b11) is always used in the case of aggregate messages where, in the example above, the second message will start with an id of 0xC3 (b11000011) and would mean that all Message Header fields should be derived from the message with a stream Id of 3 (which would be the message right above it). The six least significant bits that form the Stream ID can take values between 3 and 63. Some values have special meaning, like 1 that stands for an extended ID format, in which case there will be two bytes following that. A value of two is for low level messages such as Ping and Set Client Bandwidth.

The next bytes of the RTMP Header (including the values in the example packet above) are decoded as follows:

byte #1 (0x03) = Chunk Header Type.
byte #2–4 (0x000b68) = Timestamp delta.
byte #5–7 (0x000019) = Packet Length - in this case it is 0x000019 = 25 bytes.
byte #8 (0x14) = Message Type ID - 0x14 (20) defines an AMF0 encoded command message.
byte #9–12 (0x00000000) = Message Stream ID. This is in little-endian order.
The Message Type ID byte defines whether the packet contains audio/video data, a remote object or a command. Some possible values for are:

0x01 = Set Packet Size Message.
0x02 = Abort.
0x03 = Acknowledge.
0x04 = Control Message.
0x05 = Server Bandwidth
0x06 = Client Bandwidth.
0x07 = Virtual Control.
0x08 = Audio Packet.
0x09 = Video Packet.
0x0F = Data Extended.
0x10 = Container Extended.
0x11 = Command Extended (An AMF3 type command).
0x12 = Data (Invoke (onMetaData info is sent as such)).
0x13 = Container.
0x14 = Command (An AMF0 type command).
0x15 = UDP
0x16 = Aggregate
0x17 = Present
Following the header, 0x02 denotes a string of size 0x000C and values 0x63 0x72 ... 0x6D ("createStream" command). Following that we have a 0x00 (number) which is the transaction id of value 2.0. The last byte is 0x05 (null) which means there are no arguments.

Invoke Message Structure (0x14, 0x11)
Some of the message types shown above, such as Ping and Set Client/Server Bandwidth, are considered low level RTMP protocol messages which do not use the AMF encoding format. Command messages on the other hand, whether AMF0 (Message Type of 0x14) or AMF3 (0x11), use the format and have the general form shown below:

(String) <Command Name>
(Number) <Transaction Id>
(Mixed)  <Argument> ex. Null, String, Object: {key1:value1, key2:value2 ... }
The transaction id is used for commands that can have a reply. The value can be either a string like in the example above or one or more objects, each composed of a set of key/value pairs where the keys are always encoded as strings while the values can be any AMF data type, including complex types like arrays.

Control Message Structure (0x04)
Control messages are not AMF encoded. They start with a stream Id of 0x02 which implies a full (type 0) header and have a message type of 0x04. The header is followed by six bytes, which are interpreted as such:

#0–1 - Control Type.
#2–3 - Second Parameter (this has meaning in specific Control Types)
#4–5 - Third Parameter (same)
The first two bytes of the message body define the Ping Type, which can apparently[12] take six possible values.

Type 0 - Clear Stream: Sent when the connection is established and carries no further data
Type 1 - Clear the Buffer.
Type 2 - Stream Dry.
Type 3 - The client's buffer time. The third parameter holds the value in millisecond.
Type 4 - Reset a stream.
Type 6 - Ping the client from server. The second parameter is the current time.
Type 7 - Pong reply from client. The second parameter is the time when the client receives the Ping.
Type 8 - UDP Request.
Type 9 - UDP Response.
Type 10 - Bandwidth Limit.
Type 11 - Bandwidth.
Type 12 - Throttle Bandwidth.
Type 13 - Stream Created.
Type 14 - Stream Deleted.
Type 15 - Set Read Access.
Type 16 - Set Write Access.
Type 17 - Stream Meta Request.
Type 18 - Stream Meta Response.
Type 19 - Get Segment Boundary.
Type 20 - Set Segment Boundary.
Type 21 - On Disconnect.
Type 22 - Set Critical Link.
Type 23 - Disconnect.
Type 24 - Hash Update.
Type 25 - Hash Timeout.
Type 26 - Hash Request.
Type 27 - Hash Response.
Type 28 - Check Bandwidth.
Type 29 - Set Audio Sample Access.
Type 30 - Set Video Sample Access.
Type 31 - Throttle Begin.
Type 32 - Throttle End.
Type 33 - DRM Notify.
Type 34 - RTMFP Sync.
Type 35 - Query IHello.
Type 36 - Forward IHello.
Type 37 - Redirect IHello.
Type 38 - Notify EOF.
Type 39 - Proxy Continue.
Type 40 - Proxy Remove Upstream.
Type 41 - RTMFP Set Keepalives.
Type 46 - Segment Not Found.
Pong is the name for a reply to a Ping, with the values used as seen above.

ServerBw/ClientBw Message Structure (0x05, 0x06)
This relates to messages that have to do with the client up-stream and server down-stream bit-rate. The body is composed of four bytes showing the bandwidth value, with a possible extension of one byte which sets the Limit Type. This can have one of three possible values which can be: hard, soft or dynamic (either soft or hard).

Set Chunk Size (0x01)
The value received in the four bytes of the body. A default value of 128 bytes exists, and the message is sent only when a change is wanted.

Protocol

RTMP Handshake Diagram
Handshake
After establishing a TCP connection, an RTMP connection is established first, performing a handshake through the exchange of three packets from each side (also referred to as Chunks in the official documentation). These are referred in the official spec as C0-2 for the client sent packets and S0-2 for the server side respectively and are not to be confused with RTMP packets that can be exchanged only after the handshake is complete. These packets have a structure of their own and C1 contains a field setting the "epoch" timestamp, but since this can be set to zero, as is done in third party implementations, the packet can be simplified. The client initialises the connection by sending the C0 packet with a constant value of 0x03 representing the current protocol version. It follows straight with C1 without waiting for S0 to be received first which contains 1536 bytes, with the first four representing the epoch timestamp, the second four all being 0, and the rest being random (and which can be set to 0 in third party implementations). C2 and S2 are an echo of S1 and C1 respectively, except with the second four bytes being the time the respective message was received (instead of 0). After C2 and S2 are received, the handshake is considered complete.

Connect
At this point, the client, and server can negotiate a connection by exchanging AMF encoded messages. These include key value pairs which relate to variables that are needed for a connection to be established. An example message from the client is:

(Invoke) "connect"
(Transaction ID) 1.0
(Object1) { app: "sample", flashVer: "MAC 10,2,153,2", swfUrl: null,
              tcUrl: "rtmpt://127.0.0.1/sample ", fpad: false,
              capabilities: 9947.75 , audioCodecs: 3191, videoCodecs: 252,
              videoFunction: 1 , pageUrl: null, objectEncoding: 3.0 }
The Flash Media Server and other implementations uses the concept of an "app" to conceptually define a container for audio/video and other content, implemented as a folder on the server root which contains the media files to be streamed. The first variable contains the name of this app as "sample" which is the name provided by the Wowza Server for their testing. The flashVer string is the same as returned by the Action-script getversion() function. The audioCodec and videoCodec are encoded as doubles and their meaning can be found in the original spec. The same is true for the videoFunction variable, which in this case is the self-explanatory SUPPORT_VID_CLIENT_SEEK constant. Of special interest is the objectEncoding which will define whether the rest of the communication will make use of the extended AMF3 format or not. As version 3 is the current default, the flash client has to be told explicitly in Action-script code to use AMF0 if that is requested. The server then replies with a ServerBW, a ClientBW and a SetPacketSize message sequence, finally followed by an Invoke, with an example message.

(Invoke) "_result"
(transaction ID) 1.0
(Object1) { fmsVer: "FMS/3,5,5,2004", capabilities: 31.0, mode: 1.0 }
(Object2) { level: "status", code: "NetConnection.Connect.Success",
                   description: "Connection succeeded",
                   data: (array) { version: "3,5,5,2004" },
                   clientId: 1728724019, objectEncoding: 3.0 }
Some values above are serialised into properties of a generic Action-script Object, which is then passed to the NetConnection event listener. The clientId will establish a number for the session to be started by the connection. Object encoding must match the value previously set.

Play video
To start a video stream, the client sends a "createStream" invocation followed by a ping message, followed by a "play" invocation with the file name as argument. The server will then reply with a series of "onStatus" commands followed by the video data as encapsulated within RTMP messages.

After a connection is established, media is sent by encapsulating the content of FLV tags into RTMP messages of type 8 and 9 for audio and video, respectively.

Layered-on protocols
HTTP tunneling (RTMPT)
This refers to the HTTP tunneled version of the protocol. It communicates over port 80 and passes the AMF data inside HTTP POST request and responses. The sequence for connection is as follows:

POST /fcs/ident2 HTTP/1.1
Content-Type: application/x-fcs\r\n

HTTP/1.0 404 Not Found
POST /open/1 HTTP/1.1
Content-Type: application/x-fcs\r\n

HTTP/1.1 200 OK
Content-Type: application/x-fcs\r\n
    1728724019
The first request has an /fcs/ident2 path, and the correct reply is a 404 Not Found error. The client then sends an /open/1 request where the server must reply with a 200 ok appending a random number that will be used as the session identifier for the said communication. In this example, 1728724019 is returned in the response body.

POST /idle/1728724019/0 HTTP/1.1
HTTP/1.1 200 OK
   0x01
From now on, the /idle/<session id>/<sequence #> is a polling request where the session id has been generated and returned from the server and the sequence is just a number that increments by one for every request. The appropriate response is a 200 OK, with an integer returned in the body signifying the interval time. AMF data is sent through /send/<session id>/<sequence #>

Software implementations
See also: List of RTMP software
RTMP is implemented at these three stages:

Live video encoder
Live and on-demand media streaming server
Live and on-demand client
rtmpdump
Main article: rtmpdump
The open-source RTMP client command-line tool rtmpdump is designed to play back or save to disk the full RTMP stream, including the RTMPE protocol Adobe uses for encryption. RTMPdump runs on Linux, Android, Solaris, Mac OS X, and most other Unix-derived operating systems, as well as Microsoft Windows. Originally supporting all versions of 32-bit Windows including Windows 98, from version 2.2 the software will run only on Windows XP and above (although earlier versions remain fully functional).

Packages of the rtmpdump suite of software are available in the major open-source repositories (Linux distributions). These include the front-end apps "rtmpdump", "rtmpsrv" and "rtmpsuck."

Development of RTMPdump was restarted in October 2009, outside the United States, at the MPlayer site.[13] The current version features greatly improved functionality, and has been rewritten to take advantage of the benefits of the C programming language. In particular, the main functionality was built into a library (librtmp) which can easily be used by other applications. The RTMPdump developers have also written support for librtmp for MPlayer, FFmpeg, XBMC, cURL, VLC and a number of other open source software projects. Use of librtmp provides these projects with full support of RTMP in all its variants without any additional development effort.

FLVstreamer
FLVstreamer is a fork of RTMPdump, without the code, which Adobe claims violates the DMCA in the USA. This was developed as a response to Adobe's attempt in 2008 to suppress RTMPdump. FLVstreamer is an RTMP client that will save a stream of audio or video content from any RTMP server to disk, if encryption (RTMPE) is not enabled on the stream.

Extensions
The above variants are additions made on top of RTMP. However, the limited codec selections of FLV and RTMP has necessitated the industry to add things to the format and the protocol directly.

Ad hoc extension
RTMP interally uses its own four-bit "codec ID" instead of standard FourCC to identify the set of codecs it supports. Audio and video codec IDs are considered to be in different namespaces, so using the same value does not cause any conflict: for example, in Adobe's baseline standard, audio codec 7 is G.711A and video codec 7 is H.264.

In China, Kingsoft Cloud assigned H.265 to video codec "12" in 2018, a practice soon accepted by other industry players such as Bilibili. In 2020, Xia Chu of ZLMediaKit assigned Opus to audio codec 13.[14]

All ad hoc additions to the coded ID field have been made obsolete by E-RTMP. E-RTMP does not reuse the IDs 12 and 13, making it compatible with the Chinese ad hoc extensions.

Enhanced RTMP
Enhanced RTMP (E-RTMP) is an enhancement to the Real-Time Messaging Protocol (RTMP) and FLV specifications, modernizing streaming workflows while maintaining compatibility with existing RTMP infrastructure.[2] Developed as an open specification, E-RTMP was published by the Veovera Software Organization, with contributions from Adobe, Google, Twitch, and others.

Enhancements introduced in E-RTMP include:

New FLV audio and video header formats, which is signaled using reserved parts of the original header formats.
The old headers used four-bit integers to identify codec types, which limited extensibility. The new format uses FourCC numbers to identify new codecs. Standard choices include AC-3, E-AC-3, Opus, FLAC for audio and VP8, VP9, HEVC, and AV1 with HDR capabilities for video, though any codec with an agreed-upon FourCC identifier can be used.
The new headers include nanosecond-level timestamps for better synchronization with modern media formats.
The new headers include the ability to describe a multitrack configuration, allowing simultaneous audio, video, and metadata processing within a single stream.
The new audio header can describe multichannel audio configurations, which is more flexible.
The new video header uses packet-types instead of frame-types. One of these types, VideoPacketType.Metadata, allows writing any video-metadata in Action Message Format (AMF). (AMF is used in the original RTMP on the protocol-messaging layer, not as part of the FLV video stream.)
Advertisement of new codecs capabilities in FourCC format.
New fields for onMetaData, an existing metadata mechanism in RTMP (the existing use of AMF).
A reconnect request feature to improve connection stability and resilience in streaming workflows.
E-RTMP enhances RTMP’s capabilities while ensuring interoperability with existing RTMP implementations. For example, a legacy decoder would not know how to decode frames containing reserved ("new") headers, but it would still be able to forward it properly.

Because many changes in E-RTMP are made to the FLV format layer, E-RTMP also includes an extension to the FLV file format that can be used independently of E-RTMP. ffmpeg calls this format "enhanced FLV".[15]

Implementations
Producers:

ffmpeg (since 6.1)[15]
OBS Studio 29.1[16]
Ant Media Server[17]
Ingesters:

YouTube (Beta, same ingest parameters as h264)
Twitch[18]
Ingesters, Enhanced-FLV only:

e-flv-js, Veovera's fork of flv-js (originally from Bilibili). The author of flv-js also has his own fork mpegts.js with some Enhanced-FLV support.[19]
See also
icon	Internet portal
Secure Reliable Transport (SRT)
Reliable Internet Stream Transport (RIST)
WebRTC
Protected Streaming Info about RTMPS and RTMPE
Video on Demand (VoD)
Media Source Extensions (MSE)
WebSocket

---

The Real-time Transport Protocol (RTP) is a network protocol for delivering audio and video over IP networks. RTP is used in communication and entertainment systems that involve streaming media, such as telephony, video teleconference applications including WebRTC, television services and web-based push-to-talk features.

RTP typically runs over User Datagram Protocol (UDP). RTP is used in conjunction with the RTP Control Protocol (RTCP). While RTP carries the media streams (e.g., audio and video), RTCP is used to monitor transmission statistics and quality of service (QoS) and aids synchronization of multiple streams. RTP is one of the technical foundations of voice over IP and in this context is often used in conjunction with a signaling protocol such as the Session Initiation Protocol (SIP), which establishes connections across the network.

RTP was developed by the Audio-Video Transport Working Group of the Internet Engineering Task Force (IETF) and first published in 1996 as RFC 1889, which was then superseded by RFC 3550 in 2003.[2]

Overview
Research on audio and video over packet-switched networks dates back to the early 1970s. The Internet Engineering Task Force (IETF) published RFC 741 in 1977 and began developing RTP in 1992,[1] and would go on to develop Session Announcement Protocol (SAP), the Session Description Protocol (SDP), and the Session Initiation Protocol (SIP).

RTP is designed for end-to-end, real-time transfer of streaming media. The protocol provides facilities for jitter compensation and detection of packet loss and out-of-order delivery, which are common, especially during UDP transmissions on an IP network. RTP allows data transfer to multiple destinations through IP multicast.[3] RTP is regarded as the primary standard for audio/video transport in IP networks and is used with an associated profile and payload format.[4][needs update] The design of RTP is based on the architectural principle known as application-layer framing, where protocol functions are implemented in the application as opposed to the operating system's protocol stack.

Real-time multimedia streaming applications require timely delivery of information and often can tolerate some packet loss to achieve this goal. For example, loss of a packet in an audio application may result in loss of a fraction of a second of audio data, which can be made unnoticeable with suitable error concealment algorithms.[5] The Transmission Control Protocol (TCP), although standardized for RTP use,[6] is not normally used in RTP applications because TCP favors reliability over timeliness. Instead, the majority of the RTP implementations are built on the User Datagram Protocol (UDP).[5] Other transport protocols specifically designed for multimedia sessions are SCTP[7] and DCCP,[8] although, as of 2012, they were not in widespread use.[9]

RTP was developed by the Audio/Video Transport working group of the IETF standards organization. RTP is used in conjunction with other protocols such as H.323 and RTSP.[4] The RTP specification describes two protocols: RTP and RTCP. RTP is used for the transfer of multimedia data, and the RTCP is used to periodically send control information and QoS parameters.[10]

The data transfer protocol, RTP, carries real-time data. Information provided by this protocol includes timestamps (for synchronization), sequence numbers (for packet loss and reordering detection) and the payload format, which indicates the encoded format of the data.[11] The control protocol, RTCP, is used for quality of service (QoS) feedback and synchronization between the media streams. The bandwidth of RTCP traffic compared to RTP is small, typically around 5%.[11][12]

RTP sessions are typically initiated between communicating peers using a signaling protocol, such as H.323, the Session Initiation Protocol (SIP), RTSP, or Jingle (XMPP). These protocols may use the Session Description Protocol to specify the parameters for the sessions.[13]

An RTP session is established for each multimedia stream. Audio and video streams may use separate RTP sessions, enabling a receiver to selectively receive components of a particular stream.[14] The RTP and RTCP design is independent of the transport protocol. Applications most typically use UDP with port numbers in the unprivileged range (1024 to 65535).[15] The Stream Control Transmission Protocol (SCTP) and the Datagram Congestion Control Protocol (DCCP) may be used when a reliable transport protocol is desired. The RTP specification recommends even port numbers for RTP and the use of the next odd port number for the associated RTCP session.[16]: 68  A single port can be used for RTP and RTCP in applications that multiplex the protocols.[17]

RTP is used by real-time multimedia applications such as voice over IP, audio over IP, WebRTC, Internet Protocol television, and professional video over IP including SMPTE 2022 and SMPTE 2110.

Profiles and payload formats
Main article: RTP payload formats
RTP is designed to carry a multitude of multimedia formats, which permits the development of new formats without revising the RTP standard. To this end, the information required by a specific application of the protocol is not included in the generic RTP header. For each class of application (e.g., audio, video), RTP defines a profile and associated payload formats.[10] Every instantiation of RTP in a particular application requires a profile and payload format specifications.[18]: 71 

The profile defines the codecs used to encode the payload data and their mapping to payload format codes in the protocol field Payload Type (PT) of the RTP header. Each profile is accompanied by several payload format specifications, each of which describes the transport of particular encoded data.[4] Examples of audio payload formats are G.711, G.723, G.726, G.729, GSM, QCELP, MP3, and DTMF, and examples of video payloads are H.261, H.263, H.264, H.265 and MPEG-1/MPEG-2.[19] The mapping of MPEG-4 audio/video streams to RTP packets is specified in RFC 3016, and H.263 video payloads are described in RFC 2429.[20]

Examples of RTP profiles include:

The RTP profile for Audio and video conferences with minimal control (RFC 3551) defines a set of static payload type assignments, and a dynamic mechanism for mapping between a payload format and a PT value using Session Description Protocol (SDP).
The Secure Real-time Transport Protocol (SRTP) (RFC 3711) defines an RTP profile that provides cryptographic services for the transfer of payload data.[21]
The experimental Control Data Profile for RTP (RTP/CDP) for machine-to-machine communications.[22]
Packet header
RTP packets are created at the application layer and handed to the transport layer for delivery. Each unit of RTP media data created by an application begins with the RTP packet header.

RTP packet header
Offset	Octet	0	1	2	3
Octet	Bit	0	1	2	3	4	5	6	7	8	9	10	11	12	13	14	15	16	17	18	19	20	21	22	23	24	25	26	27	28	29	30	31
0	0	Version	P	X	CC	M	PT	Sequence Number
4	32	Timestamp
8	64	SSRC Identifier
12	96	CSRC Identifier(s)
⋮	⋮	⋮
⁠
12
+
4
×
C
C
{\displaystyle 12+4\times \mathrm {CC} }⁠	⁠
96
+
32
×
C
C
{\displaystyle 96+32\times \mathrm {CC} }⁠	Profile-specific Extension Header ID	Extension Header Length
⁠
16
+
4
×
C
C
{\displaystyle 16+4\times \mathrm {CC} }⁠	⁠
128
+
32
×
C
C
{\displaystyle 128+32\times \mathrm {CC} }⁠	Extension Data
⋮	⋮	⋮
The RTP header has a minimum size of 12 bytes. After the header, optional header extensions may be present. This is followed by the RTP payload, the format of which is determined by the particular class of application.[23] The fields in the header are as follows:

Version: 2 bits
Indicates the version of the protocol. Current version is 2.[24]
Padding (P): 1 bit
Used to indicate if there are extra padding bytes at the end of the RTP packet. Padding may be used to fill up a block of certain size, for example, as required by an encryption algorithm. The last byte of the padding contains the number of padding bytes that were added (including itself).[16]: 12 [24]
Extension (X): 1 bit
Indicates presence of an Extension Header between the header and payload data. The extension header is application or profile specific.[24]
CSRC Count (CC): 4 bits
Contains the number of CSRC identifiers (defined below) that follow the SSRC (also defined below).[16]: 12 
Marker (M): 1 bit
Signaling used at the application level in a profile-specific manner. If it is set, it means that the current data has some special relevance for the application.[16]: 13 
Payload Type (PT): 7 bits
Indicates the format of the payload and thus determines its interpretation by the application. Values are profile specific and may be dynamically assigned.[25]
Sequence Number: 16 bits
The sequence number is incremented for each RTP data packet sent and is to be used by the receiver to detect packet loss[3] and to accommodate out-of-order delivery. The initial value of the sequence number should be randomized to make known-plaintext attacks on Secure Real-time Transport Protocol more difficult.[16]: 13 
Timestamp: 32 bits
Used by the receiver to play back the received samples at appropriate time and interval. When several media streams are present, the timestamps may be independent in each stream.[a] The granularity of the timing is application specific. For example, an audio application that samples data once every 125 μs (8 kHz, a common sample rate in digital telephony) would use that value as its clock resolution. Video streams typically use a 90 kHz clock. The clock granularity is one of the details that is specified in the RTP profile for an application.[26]
SSRC: 32 bits
Synchronization Source Identifier uniquely identifies the source of a stream. The synchronization sources within the same RTP session will be unique.[16]: 15 
CSRC: Variable (CSRC Count × 32 bits)
Contributing Source IDs enumerate contributing sources to a stream that has been generated from multiple sources.[16]: 15 
Header Extension: Variable; Exists when X=1
When Extension is true, this optional field contains:
Profile-specific Extension Header ID: 16 bits
a profile-specific identifier
Extension Header Length: 16 bits
indicates the length of the extension in 32-bit units, excluding the 32 bits of the extension header.
Extension Header Data: Variable
The data of the extension header.[16]: 18 
Application design
A functional multimedia application requires other protocols and standards used in conjunction with RTP. Protocols such as SIP, Jingle, RTSP, H.225 and H.245 are used for session initiation, control and termination. Other standards, such as H.264, MPEG and H.263, are used for encoding the payload data as specified by the applicable RTP profile.[27]

An RTP sender captures the multimedia data, then encodes, frames and transmits it as RTP packets with appropriate timestamps and increasing timestamps and sequence numbers. The sender sets the payload type field in accordance with connection negotiation and the RTP profile in use. The RTP receiver detects missing packets and may reorder packets. It decodes the media data in the packets according to the payload type and presents the stream to its user.[27]

Standards documents
RFC 3550 – "RTP: A Transport Protocol for Real-Time Applications,"[16] Internet Standard 64.
RFC 3551 – "RTP Profile for Audio and Video Conferences with Minimal Control,"[28] Internet Standard 65.
RFC 4855 – "Media Type Registration of RTP Payload Formats,"[29] Proposed Standard.
RFC 4856 – "Media Type Registration of Payload Formats in the RTP Profile for Audio and Video Conferences,"[30] Proposed Standard.
RFC 7656 – "A Taxonomy of Semantics and Mechanisms for Real-Time Transport Protocol (RTP) Sources,"[31] Informational.
RFC 3190 – "RTP Payload Format for 12-bit DAT Audio and 20- and 24-bit Linear Sampled Audio,"[32] Proposed Standard.
RFC 6184 – "RTP Payload Format for H.264 Video,"[33] Proposed Standard.
RFC 3640 – "RTP Payload Format for Transport of MPEG-4 Elementary Streams,"[34] Proposed Standard.
RFC 6416 – "RTP Payload Format for MPEG-4 Audio/Visual Streams,"[35] Proposed Standard.
RFC 2250 – "RTP Payload Format for MPEG1/MPEG2 Video,"[36] Proposed Standard.
RFC 4175 – "RTP Payload Format for Uncompressed Video,"[37] Proposed Standard.
RFC 6295 – "RTP Payload Format for MIDI,"[38] Proposed Standard.
RFC 4696 – "An Implementation Guide for RTP MIDI,"[39] Informational.
RFC 7164 – "RTP and Leap Seconds,"[40] Proposed Standard.
RFC 7587 – "RTP Payload Format for the Opus Speech and Audio Codec,"[41] Proposed Standard.
RFC 7798 – "RTP Payload Format for High Efficiency Video Coding (HEVC),"[42] Proposed Standard.
See also
Real Data Transport
Real Time Streaming Protocol
ZRTP

---

Not to be confused with Rapid Spanning Tree Protocol or Real-Time Messaging Protocol.
icon
This article needs more citations. Please help improve this article by adding citations to reliable sources. Unsourced material may be challenged and removed.
Find sources: "Real-Time Streaming Protocol" – news · newspapers · books · scholar · JSTOR (September 2013) (Learn how and when to remove this message)
Real Time Streaming Protocol
Communication protocol
Abbreviation	RTSP
Purpose	Internet streaming
Developers	RealNetworks, Netscape, Columbia University
Introduction	April 1998; 28 years ago
OSI layer	Application layer (7)
Ports	
554/TCP
554/UDP
RFCs	RFC 2326, 7826
The Real-Time Streaming Protocol (RTSP) is an application-level network protocol designed for multiplexing and packetizing multimedia transport streams (such as interactive media, video and audio) over a suitable transport protocol.

RTSP is used in entertainment and communications systems to control streaming media servers.

The protocol is used for establishing and controlling media sessions between endpoints.

Clients of media servers issue commands such as play, record and pause to facilitate real-time control of the media streaming from the server to a client (video on demand) or from a client to the server (voice recording).

History
RTSP was developed by RealNetworks, Netscape and Columbia University.[1][2] The first draft of the protocol was submitted to IETF in October 1996 by Netscape and Progressive Networks, after which Henning Schulzrinne from Columbia University submitted "RTSP՚" ("RTSP prime") in December 1996.[3][4] The two drafts were merged for standardization by the Multiparty Multimedia Session Control Working Group (MMUSIC WG) of the Internet Engineering Task Force (IETF), and further drafts were published by the working group.[5][6] RTSP was published as RFC 2326 in 1998.[7]

RTSP 2.0 was published as RFC 7826 in 2016 as a replacement of RTSP 1.0. Version 2.0 is based on version 1.0 but is not backwards compatible (other than in the basic version negotiation mechanism).[8]

Internet protocol suite
Application layer
BGPDHCP (v6)DNSFTPHTTP (HTTP/3)HTTPSIMAPJMAPIPPIRCLDAPMGCPMQTTNNTPNTPOSPFPOPPTPONC/RPCRTPRTSPRIPSIPSMTPSNMPSSHTelnetTLS/SSLXMPPmore...
Transport layer
TCPUDPDCCPSCTPRSVPQUICmore...
Internet layer
IP v4v6ICMP (v6)NDPECNL4SIGMPIPsecmore...
Link layer
ARPTunnelsPPPMACmore...
vte
RTP
Main article: Real-time Transport Protocol
The transmission of streaming data itself is not a task of RTSP. Most RTSP servers use the Real-time Transport Protocol (RTP) in conjunction with Real-time Control Protocol (RTCP) for media stream delivery. However, some vendors implement proprietary transport protocols. The RTSP server software from RealNetworks, for example, also used RealNetworks' proprietary Real Data Transport (RDT).

Protocol directives
While similar in some ways to HTTP, RTSP defines control sequences that are useful in controlling multimedia playback. While HTTP is stateless, RTSP has a state; an identifier is used to track concurrent sessions when needed. Like HTTP, RTSP uses TCP to maintain an end-to-end connection. While most RTSP control messages are sent by the client to the server, some commands travel in the other direction (i.e., from server to client).

Presented below are the basic RTSP requests. Some typical HTTP requests, like the OPTIONS request, are also available. The default transport layer port number is 554[7] for both TCP and UDP, but the latter is rarely used for control requests.

OPTIONS
An OPTIONS request returns the request types the server will accept.
C->S:  OPTIONS rtsp://example.com/media.mp4 RTSP/1.0
       CSeq: 1
       Require: implicit-play
       Proxy-Require: gzipped-messages

S->C:  RTSP/1.0 200 OK
       CSeq: 1
       Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE
DESCRIBE
A DESCRIBE request includes an RTSP rtsp:// URL, and the type of reply data that can be handled. This reply includes the presentation description, typically in Session Description Protocol (SDP) format. Among other things, the presentation description lists the media streams controlled with the aggregate URL. In a typical case, there is one media stream each for audio and video streams. The media stream URLs are either obtained directly from the SDP control fields or by appending the SDP control field to the aggregate URL.
C->S: DESCRIBE rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 2

S->C: RTSP/1.0 200 OK
      CSeq: 2
      Content-Base: rtsp://example.com/media.mp4
      Content-Type: application/sdp
      Content-Length: 460

      m=video 0 RTP/AVP 96
      a=control:streamid=0
      a=range:npt=0-7.741000
      a=length:npt=7.741000
      a=rtpmap:96 MP4V-ES/5544
      a=mimetype:string;"video/MP4V-ES"
      a=AvgBitRate:integer;304018
      a=StreamName:string;"hinted video track"
      m=audio 0 RTP/AVP 97
      a=control:streamid=1
      a=range:npt=0-7.712000
      a=length:npt=7.712000
      a=rtpmap:97 mpeg4-generic/32000/2
      a=mimetype:string;"audio/mpeg4-generic"
      a=AvgBitRate:integer;65790
      a=StreamName:string;"hinted audio track"
SETUP
A SETUP request specifies how a single media stream will be transported. This must be done before a PLAY request is sent. The request contains the media stream URL and a transport specifier. This specifier typically includes a local port for receiving RTP data (audio or video), and another for RTCP data (meta information). The server reply usually confirms the chosen parameters and fills in the missing parts, such as the server's chosen ports. Each media stream must be configured using SETUP before an aggregate play request may be sent.
C->S: SETUP rtsp://example.com/media.mp4/streamid=0 RTSP/1.0
      CSeq: 3
      Transport: RTP/AVP;unicast;client_port=8000-8001

S->C: RTSP/1.0 200 OK
      CSeq: 3
      Transport: RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001;ssrc=1234ABCD
      Session: 12345678

C->S: SETUP rtsp://example.com/media.mp4/streamid=1 RTSP/1.0
      CSeq: 3
      Transport: RTP/AVP;unicast;client_port=8002-8003
      Session: 12345678

S->C: RTSP/1.0 200 OK
      CSeq: 3
      Transport: RTP/AVP;unicast;client_port=8002-8003;server_port=9002-9003;ssrc=1234ABCD
      Session: 12345678
PLAY
A PLAY request will cause one or all media streams to be played. Play requests can be stacked by sending multiple PLAY requests. The URL may be the aggregate URL (to play all media streams), or a single media stream URL (to play only that stream). A range can be specified. If no range is specified, the stream is played from the beginning and plays to the end, or if the stream is paused, it is resumed at the point it was paused.
C->S: PLAY rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 4
      Range: npt=5-20
      Session: 12345678

S->C: RTSP/1.0 200 OK
      CSeq: 4
      Session: 12345678
      RTP-Info: url=rtsp://example.com/media.mp4/streamid=0;seq=9810092;rtptime=3450012
PAUSE
A PAUSE request temporarily halts one or all media streams, to be resumed later via a PLAY request. The request contains an aggregate or media stream URL. A range parameter on a PAUSE request specifies when to pause. When the range parameter is omitted, the pause occurs immediately and indefinitely.
C->S: PAUSE rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 5
      Session: 12345678

S->C: RTSP/1.0 200 OK
      CSeq: 5
      Session: 12345678
RECORD
This method initiates recording a range of media data according to the presentation description. The timestamp reflects the start and end time (in UTC). If no time range is given, use the start or end time provided in the presentation description. If the session has already started, recording commences immediately. The server decides whether to store the recorded data under the request URI or another URI. If the server does not use the request URI, the response should be 201 and contain an entity that describes the states of the request and refers to the new resource, as well as containing a Location header.
C->S: RECORD rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 6
      Session: 12345678

S->C: RTSP/1.0 200 OK
      CSeq: 6
      Session: 12345678
ANNOUNCE
The ANNOUNCE method serves two purposes:

When sent from client to server, ANNOUNCE posts the description of a presentation or media object identified by the request URL to a server. When sent from server to client, ANNOUNCE updates the session description in real time. If a new media stream is added to a presentation (e.g., during a live presentation), the whole presentation description must be sent again, rather than just the additional components, so that components can be deleted.

C->S: ANNOUNCE rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 7
      Date: 23 Jan 1997 15:35:06 GMT
      Session: 12345678
      Content-Type: application/sdp
      Content-Length: 332

      v=0
      o=mhandley 2890844526 2890845468 IN IP4 126.16.64.4
      s=SDP Seminar
      i=A Seminar on the session description protocol
      u=http://www.cs.ucl.ac.uk/staff/M.Handley/sdp.03.ps
      e=mjh@isi.edu (Mark Handley)
      c=IN IP4 224.2.17.12/127
      t=2873397496 2873404696
      a=recvonly
      m=audio 3456 RTP/AVP 0
      m=video 2232 RTP/AVP 31

S->C: RTSP/1.0 200 OK
      CSeq: 7
TEARDOWN
A TEARDOWN request is used to terminate the session. It stops all media streams and frees all session-related data on the server.
C->S: TEARDOWN rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 8
      Session: 12345678

S->C: RTSP/1.0 200 OK
      CSeq: 8
GET_PARAMETER
The GET_PARAMETER request retrieves the value of a parameter of a presentation or stream specified in the URI. The content of the reply and response is left to the implementation. GET_PARAMETER with no entity body may be used to test client or server liveness ("ping").
S->C: GET_PARAMETER rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 9
      Content-Type: text/parameters
      Session: 12345678
      Content-Length: 15

      packets_received
      jitter

C->S: RTSP/1.0 200 OK
      CSeq: 9
      Content-Length: 46
      Content-Type: text/parameters

      packets_received: 10
      jitter: 0.3838
SET_PARAMETER
This method requests to set the value of a parameter for a presentation or stream specified by the URI.
C->S: SET_PARAMETER rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 10
      Content-length: 20
      Content-type: text/parameters

      barparam: barstuff

S->C: RTSP/1.0 451 Invalid Parameter
      CSeq: 10
      Content-length: 10
      Content-type: text/parameters

      barparam
REDIRECT
A REDIRECT request informs the client that it must connect to another server location. It contains the mandatory header Location, which indicates that the client should issue requests for that URL. It may contain the parameter Range, which indicates when the redirection takes effect. If the client wants to continue to send or receive media for this URI, the client MUST issue a TEARDOWN request for the current session and a SETUP for the new session at the designated host.
S->C: REDIRECT rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 11
      Location: rtsp://bigserver.com:8001
      Range: clock=19960213T143205Z-
Embedded (Interleaved) Binary Data
Certain firewall designs or other circumstances may force a server to interleave RTSP methods and stream data. This interleaving should generally be avoided unless necessary, since it complicates client and server operation and imposes additional overhead. Interleaved binary data should only be used if RTSP is carried over TCP.
Stream data, such as RTP packets, is encapsulated by an ASCII dollar sign (24 hexadecimal), followed by a one-byte channel identifier, followed by the length of the encapsulated binary data as a binary, two-byte integer in network byte order. The stream data follows immediately afterwards, without a CRLF, but including the upper-layer protocol headers. Each $ block contains exactly one upper-layer protocol data unit, e.g., one RTP packet.

C->S: SETUP rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 3
      Transport: RTP/AVP/TCP;interleaved=0-1

S->C: RTSP/1.0 200 OK
      CSeq: 3
      Date: 05 Jun 1997 18:57:18 GMT
      Transport: RTP/AVP/TCP;interleaved=0-1
      Session: 12345678

C->S: PLAY rtsp://example.com/media.mp4 RTSP/1.0
      CSeq: 4
      Session: 12345678

S->C: RTSP/1.0 200 OK
      CSeq: 4
      Session: 12345678
      Date: 05 Jun 1997 18:59:15 GMT
      RTP-Info: url=rtsp://example.com/media.mp4;seq=232433;rtptime=972948234

S->C: $\000{2 byte length}{"length" bytes data, w/RTP header}
S->C: $\000{2 byte length}{"length" bytes data, w/RTP header}
S->C: $\001{2 byte length}{"length" bytes  RTCP packet}
RTSP over HTTP
RTSP over HTTP was defined by Apple in 1999.[9] It interleaves the RTP Video and Audio data into the RTSP Command Connection (as defined in RFC2326), and then sends the RTSP Command Connection via a pair of HTTP connections. One of these is a long-running GET connection, and the other is a long-running POST connection.

This method is also used in the ONVIF IP Camera standard, and can be combined with HTTPS for secure and encrypted video and audio.

RTSP Encryption and RTSPS
There are several different methods for encrypting RTSP command messages and RTP Video and Audio data.

RTSP 2.0 (RFC7826) defines several methods for encryption and introduces a new rtsps:// URL. Many of these have been incorporated into RFC2326 RTSP 1.0 Clients and Servers.

RTSPS URL (using the rtsps:// URL) - This method uses a TLS Socket (default of Port 322) to establish an encrypted connection between the RTSP client and the RTSP Server.
Video and Audio can then sent in one of two ways:
TCP Video/Audio - The RTP Video and Audio is sent interleaved with the RTSP Commands over the already encrypted TLS connection.
UDP and Multicast-UDP Video/Audio - the RTP Video and Audio is encrypted using the Secure RTP (SRTP) protocol and sent in parallel to the RTSPS TLS connection.
RTSP over HTTPS - this method interleaves the RTP Video and Audio data into the RTSP Command Connection (as defined in RFC2326) and then sends the RTSP Command Connection via a pair of encrypted HTTPS connections. It uses Port 443 by default.
IANA have reserved the rtsps:// URL prefix and Port 322 for RTSPS.[10] As of September 2024, RTSP over HTTPS has been implemented in several ONVIF IP Cameras and RTSPS (using the rtsps:// URL) has been implemented by Axis and Bosch CCTV Cameras,[11] FFmpeg, GStreamer, MediaMTX,[12] Ant Media Server[13] and SharpRTSP.[14]

Rate adaptation
RTSP using RTP and RTCP allows for the implementation of rate adaptation.[15]

Implementations
Server
Ant Media Server: The Community version[16] supports pulling from RTSP/S sources and can livestream using HLS or record in MP4. The Enterprise version can convert RTSP to WebRTC with approximately 200-500ms end-to-end latency.
Darwin Streaming Server is an open source version of QuickTime Streaming Server maintained by Apple.
GStreamer based RTSP Server and client.
Helix DNA Server is RealNetworks' streaming server, which comes in both open-source and proprietary versions.
Helix Universal Server is RealNetworks' commercial streaming server for RTSP, RTMP, iOS, Silverlight and HTTP streaming media clients.
LIVE555 liveMedia / openRTSP are open source C++ server and client libraries used in well-known clients like VLC and mplayer.
Motion is a free CCTV software application for Linux.
Nimble Streamer supports RTSP pull and announce input, with TCP interleaved playback output.
OvenMediaEngine is an open source low-latency streaming server that supports RTSP push input.
pvServer: Formerly called PacketVideo Streaming Server, this is Alcatel-Lucent's streaming server product.
QuickTime Streaming Server is Apple's closed-source streaming server that ships with Mac OS X Server.
VideoLAN is an open source media player and streaming server.
Windows Media Services is a Microsoft streaming server previously included with Windows Server that uses RTSP modified with Windows Media extensions.
Wowza Streaming Engine is a multi-format streaming server for RTSP/RTP, RTMP, MPEG-TS, ICY, HTTP (HTTP Live Streaming, HTTP Dynamic Streaming, Smooth Streaming, MPEG-DASH), and WebRTC.
YouTube implemented a mobile web front end in June 2007, which serves video through this protocol.[17]
Many CCTV / Security cameras, often called IP cameras, support RTSP streaming too, especially those with ONVIF profiles G, S, T.

Client
Astra
cURL (beginning with version 7.20.0—9 February 2010[18])
FFmpeg[19]
GStreamer
JetAudio
LIVE555 liveMedia / openRTSP: Open source C++ server and client libraries used in well-known clients like VLC and mplayer.
Media Player Classic
MPlayer
MythTV via Freebox
QuickTime
RealPlayer
Skype
Spotify
VLC media player
Winamp
Windows Media Player
xine
ZoneMinder
Motion (surveillance software)

---

MQTT is a lightweight, publish–subscribe, machine-to-machine network protocol for message queueing/message queueing services. It is designed for connections with remote locations that have devices with resource constraints or limited network bandwidth, such as in the Internet of things (IoT). It must run over a transport protocol that provides ordered, lossless, bi-directional connections—typically, TCP/IP.[1] It is an open OASIS standard and an ISO recommendation (ISO/IEC 20922).

History
Andy Stanford-Clark (IBM) and Arlen Nipper (then working for Arcom Control Systems) authored the first version of the protocol in 1999.[5] It was used to monitor oil pipelines within the SCADA industrial control system.[6] The goal was to have a protocol that is bandwidth-efficient, lightweight and uses little battery power, because the devices were connected via satellite link, which was extremely expensive at that time.[7]

Historically, the "MQ" in "MQTT" came from the IBM MQ (then "MQSeries") product line, where it stands for "Message Queue". However, the protocol provides publish-and-subscribe messaging (no queues, in spite of the name).[8] In the specification opened by IBM, as version 3.1, the protocol was referred to as "MQ Telemetry Transport".[9][10] Subsequent versions released by OASIS strictly refer to the protocol as just "MQTT", although the technical committee itself is named "OASIS Message Queuing Telemetry Transport Technical Committee".[3] Since 2013, "MQTT" does not stand for anything.[11][8]

In 2013, IBM submitted MQTT v3.1 to the OASIS specification body with a charter that ensured only minor changes to the specification could be accepted.[3] After taking over maintenance of the standard from IBM, OASIS released version 3.1.1 on October 29, 2014.[12][13] A more substantial upgrade to MQTT version 5, adding several new features,[14] was released on March 7, 2019.[1]

Derived specifications
MQTT-SN (MQTT for Sensor Networks) is a variation of the main protocol aimed at battery-powered embedded devices on non-TCP/IP networks. Any network that provides for bidirectional transport may be used[15] (such as UDP and serial links),[16] though Zigbee is the original design target.[15] Communication within a MQTT-SN network as well as to the outside MQTT network is coordinated by a "MQTT-SN Gateway".[15]

Overview
The MQTT protocol defines two types of network entities: a message broker and a number of clients. An MQTT broker is a server that receives messages from publishing clients and then routes the messages to the appropriate destination clients.[17] An MQTT client is any device (from a microcontroller up to a full-fledged server) that runs an MQTT library and connects to an MQTT broker over a network.[18]

Information is organized in a hierarchy of topics. When a publisher has a new item of data to distribute, it sends a control message with the data to the connected broker. The broker then distributes the information to any clients that have subscribed to that topic. The publisher does not need to have any data on the number or locations of subscribers; and subscribers, in turn, do not have to be configured with any data about the publishers.

If a broker receives a message on a topic for which there are no current subscribers, the broker discards the message unless the publisher of the message designated the message as a retained message. A retained message is a normal MQTT message with the retained flag set to true. The broker stores the last retained message and the corresponding quality of service (QoS) for the selected topic. Each client that subscribes to a topic pattern that matches the topic of the retained message receives the retained message immediately after they subscribe. The broker stores only one retained message per topic.[19] This allows new subscribers to a topic to receive the most current value rather than waiting for the next update from a publisher.

When a publishing client first connects to the broker, it can set up a default message to be sent to subscribers if the broker detects that the publishing client has unexpectedly disconnected from the broker.

Clients only interact with a broker, but a system may contain several broker servers that exchange data based on their current subscribers' topics.

A minimal MQTT control message can be as little as two bytes of data. A control message can carry nearly 256 megabytes of data if needed. There are 14 defined message types used to connect and disconnect a client from a broker, to publish data, to acknowledge receipt of data, and to supervise the connection between client and server.

MQTT relies on the TCP protocol for data transmission. A variant, MQTT-SN, is used over other transports such as UDP or Bluetooth.

MQTT sends connection credentials in plain text format and does not include any measures for security or authentication. This can be provided by using TLS to encrypt and protect the transferred information against interception, modification or forgery.

The default unencrypted MQTT port is 1883. The encrypted port is 8883.[20]

MQTT broker
The MQTT broker is a piece of software running on a computer (running on-premises or in the cloud), and could be self-built or hosted by a third party. It is available in both open source and proprietary implementations.

The broker acts as a post office. MQTT clients do not use a direct connection address of the intended recipient, but use the subject line called "Topic". Anyone who subscribes receives a copy of all messages for that topic. Multiple clients can subscribe to a topic from a single broker (one to many capability), and a single client can register subscriptions to topics with multiple brokers (many to one).

Each client can both produce and receive data by both publishing and subscribing, i.e. the devices can publish sensor data and still be able to receive the configuration information or control commands (MQTT is a bi-directional communication protocol). This helps in both sharing data, managing and controlling devices. A client cannot broadcast the same data to a range of topics, and must publish multiple messages to the broker, each with a single topic given.

With the MQTT broker architecture, the client devices and server application become decoupled. In this way, the clients are kept unaware of each other's information. MQTT, if configured to, can use TLS encryption with certificate, username and password protected connections. Optionally, the connection may require certification, in the form of a certificate file that a client provides and must match with the server's copy.

In case of failure, the broker software and clients can automatically hand over to a redundant/automatic backup broker. Backup brokers can also be set up to share the load of clients across multiple servers on site, in the cloud, or a combination of these.

The broker can support both standard MQTT and MQTT for compliant specifications such as Sparkplug.[21] This can be done with the same server, at the same time and with the same levels of security.

The broker keeps track of all the session's information as the device goes on and off, in a function called "persistent sessions". In this state, a broker will store both connection info for each client, topics each client has subscribed to, and any messages for a topic with a QoS of 1 or 2.[22]

The main advantages of an MQTT broker are:

Elimination of vulnerable and insecure client connections (when appropriately configured).
Ability to easily scale from a single device to thousands.
Management and tracking of client connection states, including security credentials and certificates (when appropriately configured).
Reduction of strain on cellular or satellite networks without compromising security (when appropriately configured).
Message types
Connect

Example of an MQTT connection (QoS 0) with connect, publish/subscribe, and disconnect. The first message from client B is stored due to the retain flag.
Waits for a connection to be established with the server and creates a link between the nodes.

Disconnect
Waits for the MQTT client to finish any work it must do, and for the TCP/IP session to disconnect.

Publish
Returns immediately to the application thread after passing the request to the MQTT client.

Version 5.0
In 2019, OASIS released the official MQTT 5.0 standard.[1] Version 5.0 includes the following major new features:[23]

Reason codes: Acknowledgements now support return codes, which provide a reason for a failure.
Shared subscriptions: Allow the load to be balanced across clients, thus reducing the risk of load problems.
Message expiry: Messages can include an expiry date and are deleted if they are not delivered within this time period.
Topic alias: The name of a topic can be replaced with a single number.
Quality of service
Each connection to the broker can specify a QoS measure.[24] These are classified in increasing order of overhead:

At most once – the message is sent only once and the client and broker take no additional steps to acknowledge delivery (fire and forget).
At least once – the message is re-tried by the sender multiple times until acknowledgement is received (acknowledged delivery).
Exactly once – the sender and receiver engage in a two-level handshake to ensure only one copy of the message is received (assured delivery).
This field does not affect handling of the underlying TCP data transmissions; it is only used between MQTT senders and receivers.

Security
MQTT protocol version 3.1.1 requires the server (broker) to use a timeout that is one-and-a-half times the keepalive value specified by the client. This (and similar keepalive functionalities in other protocols such as HTTP) enables a slow DoS attack published in 2020 called SlowITe, in which the attacker tries to open and maintain as many connections as possible, depriving legitimate users of their connections.[25][26] New variations of the attack and new detection/mitigation methods are topics of current research.

Clustering
The MQTT specification defines communication between a client and a single broker and does not specify communication between brokers.[1][27] Deployments that need more capacity or availability than a single broker provides rely on implementation-specific mechanisms. The two common approaches are bridging, in which one broker connects to another broker as a client and forwards messages for selected topics, and clustering, in which a group of broker nodes shares subscription and session state and appears to clients as a single logical broker.[28]

Eclipse Mosquitto implements bridging through broker configuration.[29] Clustered brokers such as EMQX and HiveMQ run as multi-node systems whose nodes share client sessions, topic subscriptions, and routing information.[30][31] A 2020 study that compared distributed deployments of three MQTT brokers on an edge-gateway cluster found substantial differences between implementations in throughput, latency, and message loss during node failure.[32]

Limitations
Message ordering
MQTT does not guarantee that messages published by independent clients arrive at subscribers in the order they were generated. The OASIS MQTT specification provides ordering guarantees only within a single client session on a single topic; events originating from separate clients or generated across independent connections may be delivered out of sequence.[33]

This limitation is particularly consequential in IoT deployments that use MQTT lifecycle events (connect and disconnect notifications) to track device connectivity state. Major broker implementations acknowledge this directly: Amazon Web Services states in its AWS IoT Core developer documentation that "lifecycle messages might be sent out of order" and that subscribers "might receive duplicate messages".[34] Similarly, HiveMQ's engineering documentation notes that "strict ordering across publishing clients requires additional strategies such as dedicated routing and sequence numbers."[35]

The practical consequence is that a disconnect notification generated before a reconnect notification may arrive at the subscribing application after it, causing monitoring systems that apply a last-write-wins policy to record a device as offline when it has already reconnected. This is a known failure mode in event-driven architectures and was characterised in the distributed systems literature as an inherent property of asynchronous message passing by Leslie Lamport in 1978.[36] Independent benchmarks have documented the operational cost of event ordering inversions in production IoT deployments.[37]

Commonly applied mitigations include debounce windows (delaying action on disconnect events for a fixed interval to allow late-arriving messages to be processed), polling or re-query patterns, and application-layer sequence numbering. Each mitigation imposes trade-offs: debounce windows introduce detection latency for genuine disconnect events and may suppress legitimate notifications; polling adds network overhead and is unsuitable for passive sensors.[38]

See also
Comparison of MQTT implementations
Advanced Message Queuing Protocol (AMQP)
Streaming Text Oriented Messaging Protocol (STOMP)
Constrained Application Protocol (CoAP)
Apache ActiveMQ
Solace PubSub+[39][40]
RabbitMQ

---

Simple Mail Transfer Protocol

Not to be confused with SMTP, Inc. or Short Message Transfer Protocol.
Simple Mail Transfer Protocol
Communication protocol

Basic flow of SMTP delivered mail
Abbreviation	SMTP
Purpose	Electronic mail transmission protocol
Introduction	November 1981; 45 years ago
OSI layer	Application layer
Ports	465, 587, 25
RFC	5321
Internet protocol suite
Application layer
BGPDHCP (v6)DNSFTPHTTP (HTTP/3)HTTPSIMAPJMAPIPPIRCLDAPMGCPMQTTNNTPNTPOSPFPOPPTPONC/RPCRTPRTSPRIPSIPSMTPSNMPSSHTelnetTLS/SSLXMPPmore...
Transport layer
TCPUDPDCCPSCTPRSVPQUICmore...
Internet layer
IP v4v6ICMP (v6)NDPECNL4SIGMPIPsecmore...
Link layer
ARPTunnelsPPPMACmore...
vte
The Simple Mail Transfer Protocol (SMTP) is an Internet standard communication protocol for electronic mail transmission. Mail servers and other message transfer agents use SMTP to send and receive mail messages. User-level email clients typically use SMTP only for sending messages to a mail server for relaying, and typically submit outgoing email to the mail server on port 465 or 587 per RFC 8314. For retrieving messages, IMAP and POP are widely used, but proprietary servers also often implement proprietary protocols, e.g., Exchange ActiveSync.

SMTP's origins began in 1980, building on concepts implemented on the ARPANET since 1971. It has been updated, modified and extended multiple times. The protocol version in common use today has extensible structure with various extensions for authentication, encryption, binary data transfer, and internationalized email addresses. SMTP servers commonly use the Transmission Control Protocol on port number 25 (between servers) and 587 (for submission from authenticated clients), both with or without encryption, and 465 with encryption for submission.

History
Further information: History of email
Predecessors to SMTP
Various forms of one-to-one electronic messaging were used in the 1960s. Users communicated using systems developed for specific mainframe computers. As more computers were interconnected, especially in the U.S. Government's ARPANET, standards were developed to permit exchange of messages between different operating systems.

Mail on the ARPANET traces its roots to 1971: the Mail Box Protocol, which was not implemented,[1] but is discussed in RFC 196; and the SNDMSG program, which Ray Tomlinson of BBN adapted that year to send messages across two computers on the ARPANET.[2][3][4] A further proposal for a Mail Protocol was made in RFC 524 in June 1973,[5] which was not implemented.[6]

The use of the File Transfer Protocol (FTP) for "network mail" on the ARPANET was proposed in RFC 469 in March 1973.[7] Through RFC 561, RFC 680, RFC 724, and finally RFC 733 in November 1977, a standardized framework for "electronic mail" using FTP mail servers on was developed.[8][9]

SMTP grew out of these standards developed during the 1970s. Ray Tomlinson discussed network mail among the International Network Working Group in INWG Protocol note 2, written in September 1974.[10] INWG discussed protocols for electronic mail in 1979,[11] which was referenced by Jon Postel in his early work on Internet email. Postel first proposed an Internet Message Protocol in 1979 as part of the Internet Experiment Note (IEN) series.[12][13][14]

Original SMTP
In 1980, Postel and Suzanne Sluizer published RFC 772 which proposed the Mail Transfer Protocol as a replacement for the use of the FTP for mail. RFC 780 of May 1981 removed all references to FTP and allocated port 57 for TCP and UDP,[15] an allocation that has since been removed by IANA. In November 1981, Postel published RFC 788 "Simple Mail Transfer Protocol".

The SMTP standard was developed around the same time as Usenet, a one-to-many communication network with some similarities.[15]

SMTP became widely used in the early 1980s. At the time, it was a complement to the Unix to Unix Copy Program (UUCP), which was better suited for handling email transfers between machines that were intermittently connected. SMTP, on the other hand, works best when both the sending and receiving machines are connected to the network all the time. Both used a store and forward mechanism and are examples of push technology. Though Usenet's newsgroups were still propagated with UUCP between servers,[16] UUCP as a mail transport has virtually disappeared[17] along with the "bang paths" it used as message routing headers.[18]

Sendmail, released with 4.1cBSD in 1983, was one of the first mail transfer agents (MTA) to implement SMTP.[19] Over time, as BSD Unix became the most popular operating system on the Internet, Sendmail became the most common mail transfer agent.[20]

The original SMTP protocol supported only unauthenticated unencrypted 7-bit ASCII text communications, susceptible to trivial man-in-the-middle attack, spoofing, and spamming, and requiring any binary data to be encoded to readable text before transmission. Due to absence of a proper authentication mechanism, by design every SMTP server was an open mail relay. The Internet Mail Consortium (IMC) reported that 55% of mail servers were open relays in 1998,[21] but less than 1% in 2002.[22] Because of spam concerns most email providers blocklist open relays,[23] making original SMTP essentially impractical for general use on the Internet.

Modern SMTP
In November 1995, RFC 1869 defined Extended Simple Mail Transfer Protocol (ESMTP), which established a general structure for all existing and future extensions which aimed to add-in the features missing from the original SMTP. ESMTP defines consistent and manageable means by which ESMTP clients and servers can be identified and servers can indicate supported extensions.

Message submission (RFC 2476) and SMTP-AUTH (RFC 2554) were introduced in 1998 and 1999, both describing new trends in email delivery. Originally, SMTP servers were typically internal to an organization, receiving mail for the organization from the outside, and relaying messages from the organization to the outside. But as time went on, SMTP servers (mail transfer agents), in practice, were expanding their roles to become message submission agents for mail user agents, some of which were now relaying mail from the outside of an organization. (e.g. a company executive wishes to send email while on a trip using the corporate SMTP server.) This issue, a consequence of the rapid expansion and popularity of the World Wide Web, meant that SMTP had to include specific rules and methods for relaying mail and authenticating users to prevent abuses such as relaying of unsolicited email (spam). Work on message submission (RFC 2476) was originally started because popular mail servers would often rewrite mail in an attempt to fix problems in it, for example, adding a domain name to an unqualified address. This behavior is helpful when the message being fixed is an initial submission, but dangerous and harmful when the message originated elsewhere and is being relayed. Cleanly separating mail into submission and relay was seen as a way to permit and encourage rewriting submissions while prohibiting rewriting relay. As spam became more prevalent, it was also seen as a way to provide authorization for mail being sent out from an organization, as well as traceability. This separation of relay and submission quickly became a foundation for modern email security practices.

As this protocol started out purely ASCII text-based, it did not deal well with binary files, or characters in many non-English languages. Standards such as Multipurpose Internet Mail Extensions (MIME) were developed to encode binary files for transfer through SMTP. Mail transfer agents (MTAs) developed after Sendmail also tended to be implemented 8-bit clean, so that the alternate "just send eight" strategy could be used to transmit arbitrary text data (in any 8-bit ASCII-like character encoding) via SMTP. Mojibake was still a problem due to differing character set mappings between vendors, although the email addresses themselves still allowed only ASCII. 8-bit-clean MTAs today tend to support the 8BITMIME extension, permitting some binary files to be transmitted almost as easily as plain text (limits on line length and permitted octet values still apply, so that MIME encoding is needed for most non-text data and some text formats). In 2012, the SMTPUTF8 extension was created to support UTF-8 text, allowing international content and addresses in non-Latin scripts like Cyrillic or Chinese.

Many people contributed to the core SMTP specifications, among them Jon Postel, Eric Allman, Dave Crocker, Ned Freed, Randall Gellens, John Klensin, and Keith Moore.

Mail processing model

Blue arrows depict implementation of SMTP variations.
Email is submitted by a mail client (mail user agent, MUA) to a mail server (mail submission agent, MSA) using SMTP on TCP port 465 or 587. Most mailbox providers still allow submission on traditional port 25. The MSA delivers the mail to its mail transfer agent (MTA). Often, these two agents are instances of the same software launched with different options on the same machine. Local processing can be done either on a single machine, or split among multiple machines; mail agent processes on one machine can share files, but if processing is on multiple machines, they transfer messages between each other using SMTP, where each machine is configured to use the next machine as a smart host. Each process is an MTA (an SMTP server) in its own right.

The boundary MTA uses DNS to look up the MX (mail exchanger) record for the recipient's domain (the part of the email address on the right of @). The MX record contains the name of the target MTA. Based on the target host and other factors, the sending MTA selects a recipient server and connects to it to complete the mail exchange.

Message transfer can occur in a single connection between two MTAs, or in a series of hops through intermediary systems. A receiving SMTP server may be the ultimate destination, an intermediate "relay" (that is, it stores and forwards the message) or a "gateway" (that is, it may forward the message using some protocol other than SMTP). Per RFC 5321 section 2.1, each hop is a formal handoff of responsibility for the message, whereby the receiving server must either deliver the message or properly report the failure to do so.

Once the final hop accepts the incoming message, it hands it to a mail delivery agent (MDA) for local delivery. An MDA saves messages in the relevant mailbox format. As with sending, this reception can be done using one or multiple computers, but in the diagram above the MDA is depicted as one box near the mail exchanger box. An MDA may deliver messages directly to storage, or forward them over a network using SMTP or other protocol such as Local Mail Transfer Protocol (LMTP), a derivative of SMTP designed for this purpose.

Once delivered to the local mail server, the mail is stored for batch retrieval by authenticated mail clients (MUAs). Mail is retrieved by end-user applications, called email clients, using Internet Message Access Protocol (IMAP), a protocol that both facilitates access to mail and manages stored mail, or the Post Office Protocol (POP) which typically uses the traditional mbox mail file format or a proprietary system such as Microsoft Exchange/Outlook or Lotus Notes/Domino. Webmail clients may use either method, but the retrieval protocol is often not a formal standard.

SMTP defines message transport, not the message content. Thus, it defines the mail envelope and its parameters, such as the envelope sender, but not the header (except trace information) nor the body of the message itself. STD 10 and RFC 5321 define SMTP (the envelope), while STD 11 and RFC 5322 define the message (header and body), formally referred to as the Internet Message Format.

Protocol overview
SMTP is a connection-oriented, text-based protocol in which a mail sender communicates with a mail receiver by issuing command strings and supplying necessary data over a reliable ordered data stream channel, typically a Transmission Control Protocol (TCP) connection. An SMTP session consists of commands originated by an SMTP client (the initiating agent, sender, or transmitter) and corresponding responses from the SMTP server (the listening agent, or receiver) so that the session is opened, and session parameters are exchanged. A session may include zero or more SMTP transactions. An SMTP transaction consists of three command/reply sequences:

MAIL command, to establish the return address, also called return-path,[24] reverse-path,[25] bounce address, mfrom, or envelope sender.
RCPT command, to establish a recipient of the message. This command can be issued multiple times, one for each recipient. These addresses are also part of the envelope.
DATA to signal the beginning of the message text; the content of the message, as opposed to its envelope. It consists of a message header and a message body separated by an empty line. DATA is actually a group of commands, and the server replies twice: once to the DATA command itself, to acknowledge that it is ready to receive the text, and the second time after the end-of-data sequence, to either accept or reject the entire message.
Besides the intermediate reply for DATA, each server's reply can be either positive (2xx reply codes) or negative. Negative replies can be permanent (5xx codes) or transient (4xx codes). A reject is a permanent failure and the client should send a bounce message to the server it received it from. A drop is a positive response followed by message discard rather than delivery.

The initiating host, the SMTP client, can be either an end-user's email client, functionally identified as a mail user agent (MUA), or a relay server's mail transfer agent (MTA), that is an SMTP server acting as an SMTP client, in the relevant session, in order to relay mail. Fully capable SMTP servers maintain queues of messages for retrying message transmissions that resulted in transient failures.

A MUA knows the outgoing mail SMTP server from its configuration. A relay server typically determines which server to connect to by looking up the MX (Mail eXchange) DNS resource record for each recipient's domain name. If no MX record is found, a conformant relaying server (not all are) instead looks up the A record. Relay servers can also be configured to use a smart host. A relay server initiates a TCP connection to the server on the "well-known port" for SMTP: port 25, or for connecting to an MSA, port 465 or 587. The main difference between an MTA and an MSA is that connecting to an MSA requires SMTP Authentication.

SMTP vs mail retrieval
SMTP is a delivery protocol only. In normal use, mail is "pushed" to a destination mail server (or next-hop mail server) as it arrives. Mail is routed based on the destination server, not the individual user(s) to which it is addressed. Other protocols, such as the Post Office Protocol (POP) and the Internet Message Access Protocol (IMAP) are specifically designed for use by individual users retrieving messages and managing mailboxes. To permit an intermittently-connected mail server to pull messages from a remote server on demand, SMTP has a feature to initiate mail queue processing on a remote server (see Remote Message Queue Starting below). POP and IMAP are unsuitable protocols for relaying mail by intermittently-connected machines; they are designed to operate after final delivery, when information critical to the correct operation of mail relay (the "mail envelope") has been removed.

Remote Message Queue Starting
Remote Message Queue Starting enables a remote host to start processing of the mail queue on a server so it may receive messages destined to it by sending a corresponding command. The original TURN command was deemed insecure and was extended in RFC 1985 with the ETRN command which operates more securely using an authentication method based on Domain Name System information.[26]

Outgoing mail SMTP server
An email client needs to know the IP address of its initial SMTP server and this has to be given as part of its configuration (usually given as a DNS name). This server will deliver outgoing messages on behalf of the user.

Outgoing mail server access restrictions
Server administrators need to impose some control on which clients can use the server. This enables them to deal with abuse, for example spam. Two solutions have been in common use:

In the past, many systems imposed usage restrictions by the location of the client, only permitting usage by clients whose IP address is one that the server administrators control. Usage from any other client IP address is disallowed.
Modern SMTP servers typically offer an alternative system that requires authentication of clients by credentials before allowing access.
Restricting access by location
icon
This section does not cite any sources. Please help improve this section by adding citations to reliable sources. Unsourced material may be challenged and removed. (October 2025) (Learn how and when to remove this message)
Under this system, an Internet service provider's (ISP's) SMTP server will not allow access by users who are outside the ISP's network. More precisely, the server may only allow access to users with an IP address provided by the ISP, which is equivalent to requiring that they are connected to the Internet using that same ISP. A mobile user may often be on a network other than that of their normal ISP, and will then find that sending email fails because the configured SMTP server choice is no longer accessible.[27]

This system has several variations. For example, an organisation's SMTP server may only provide service to users on the same network, enforcing this by firewalling to block access by users on the wider Internet. Or the server may perform range checks on the client's IP address. These methods were typically used by corporations and institutions such as universities which provided an SMTP server for outbound mail only for use internally within the organisation. However, most of these bodies now use client authentication methods, as described below.[27]

Where a user is mobile, and may use different ISPs to connect to the internet, this kind of usage restriction is onerous, and altering the configured outbound email SMTP server address is impractical. It is highly desirable to be able to use email client configuration information that does not need to change.[28]

Client authentication
Modern SMTP servers typically require authentication of clients by credentials before allowing access, rather than restricting access by location as described earlier. This more flexible system is friendly to mobile users and allows them to have a fixed choice of configured outbound SMTP server. SMTP Authentication, often abbreviated SMTP AUTH, is an extension of the SMTP in order to log in using an authentication mechanism.

Ports
Communication between mail servers generally uses the standard TCP port 25 designated for SMTP.

Mail clients however generally don't use this, instead using specific "submission" ports. Mail services generally accept email submission from clients on one of:

465 This port was deprecated after RFC 2487, until the issue of RFC 8314.
587 (Submission), as formalized in RFC 6409 (previously RFC 2476)
Port 2525 and others may be used by some individual providers, but have never been officially supported.

Many Internet service providers now block all outgoing port 25 traffic from their customers, mainly as an anti-spam measure,[29] but also to reduce costs.


SMTP transport example
A typical example of sending a message via SMTP to two mailboxes (alice and theboss) located in the same mail domain (example.com) is reproduced in the following session exchange. (In this example, the conversation parts are prefixed with S: and C:, for server and client, respectively; these labels are not part of the exchange.)

After the message sender (SMTP client) establishes a reliable communications channel to the message receiver (SMTP server), the session is opened with a greeting by the server, usually containing its fully qualified domain name (FQDN), in this case smtp.example.com. The client initiates its dialog by responding with a HELO command identifying itself in the command's parameter with its FQDN (or an address literal if none is available).[30]

S: 220 smtp.example.com ESMTP Postfix
C: HELO relay.example.org
S: 250 Hello relay.example.org, I am glad to meet you
C: MAIL FROM:<bob@example.org>
S: 250 OK
C: RCPT TO:<alice@example.com>
S: 250 OK
C: RCPT TO:<theboss@example.com>
S: 250 OK
C: DATA
S: 354 End data with <CR><LF>.<CR><LF>
C: From: "Bob Example" <bob@example.org>
C: To: "Alice Example" <alice@example.com>
C: Cc: theboss@example.com
C: Date: Tue, 15 Jan 2008 16:02:43 -0500
C: Subject: Test message
C:
C: Hello Alice.
C: This is a test message with 5 header fields and 4 lines in the message body.
C: Your friend,
C: Bob
C: .
S: 250 OK: queued as 12345
C: QUIT
S: 221 Bye
{The server closes the connection}
The client notifies the receiver of the originating email address of the message in a MAIL FROM command. This is also the return or bounce address in case the message cannot be delivered. In this example the email message is sent to two mailboxes on the same SMTP server: one for each recipient listed in the To: and Cc: header fields. The corresponding SMTP command is RCPT TO. Each successful reception and execution of a command is acknowledged by the server with a result code and response message (e.g., 250 Ok).

The transmission of the body of the mail message is initiated with a DATA command after which it is transmitted verbatim line by line and is terminated with an end-of-data sequence. This sequence consists of a new-line (<CR><LF>), a single full stop (.), followed by another new-line (<CR><LF>). Since a message body can contain a line with just a period as part of the text, the client sends two periods every time a line starts with a period; correspondingly, the server replaces every sequence of two periods at the beginning of a line with a single one. Such escaping method is called dot-stuffing.

The server's positive reply to the end-of-data, as exemplified, implies that the server has taken the responsibility of delivering the message. A message can be doubled if there is a communication failure at this time, e.g. due to a power outage: Until the sender has received that 250 Ok reply, it must assume the message was not delivered. On the other hand, after the receiver has decided to accept the message, it must assume the message has been delivered to it. Thus, during this time span, both agents have active copies of the message that they will try to deliver.[31] The probability that a communication failure occurs exactly at this step is directly proportional to the amount of filtering that the server performs on the message body, most often for anti-spam purposes. The limiting timeout is specified to be 10 minutes.[32]

The QUIT command ends the session. If the email has other recipients located elsewhere, the client would QUIT and connect to an appropriate SMTP server for subsequent recipients after the current destination(s) had been queued. The information that the client sends in the HELO and MAIL FROM commands are added (not seen in example code) as additional header fields to the message by the receiving server. It adds a Received and Return-Path header field, respectively.

Some clients are implemented to close the connection after the message is accepted (250 Ok: queued as 12345), so the last two lines may actually be omitted. This causes an error on the server when trying to send the 221 Bye reply.

SMTP Extensions
Extension discovery mechanism
Clients learn a server's supported options by using the EHLO greeting, as exemplified below, instead of the original HELO. Clients fall back to HELO only if the server does not support EHLO greeting.[33]

Modern clients may use the ESMTP extension keyword SIZE to query the server for the maximum message size that will be accepted. Older clients and servers may try to transfer excessively sized messages that will be rejected after consuming network resources, including connect time to network links that is paid by the minute.[34]

Users can manually determine in advance the maximum size accepted by ESMTP servers. The client replaces the HELO command with the EHLO command.

S: 220 smtp2.example.com ESMTP Postfix
C: EHLO bob.example.org
S: 250-smtp2.example.com Hello bob.example.org [192.0.2.201]
S: 250-SIZE 14680064
S: 250-PIPELINING
S: 250 HELP
Thus smtp2.example.com declares that it can accept a fixed maximum message size no larger than 14,680,064 octets (8-bit bytes).

In the simplest case, an ESMTP server declares a maximum SIZE immediately after receiving an EHLO. According to RFC 1870, however, the numeric parameter to the SIZE extension in the EHLO response is optional. Clients may instead, when issuing a MAIL FROM command, include a numeric estimate of the size of the message they are transferring, so that the server can refuse receipt of overly-large messages.

Binary data transfer
Original SMTP supports only a single body of ASCII text, therefore any binary data needs to be encoded as text into that body of the message before transfer, and then decoded by the recipient. Binary-to-text encodings, such as uuencode and BinHex were typically used.

The 8BITMIME command was developed to address this. It was standardized in 1994 as RFC 1652[35] It facilitates the transparent exchange of e-mail messages containing octets outside the seven-bit ASCII character set by encoding them as MIME content parts, typically encoded with Base64.

On-Demand Mail Relay
Main article: On-Demand Mail Relay
On-Demand Mail Relay (ODMR) is an SMTP extension standardized in RFC 2645 that allows an intermittently-connected SMTP server to receive email queued for it when it is connected.

Internationalization extension
Main article: International email
Original SMTP supports email addresses composed of ASCII characters only, which is inconvenient for users whose native script is not Latin based, or who use diacritic not in the ASCII character set. This limitation was alleviated via extensions enabling UTF-8 in address names. RFC 5336 introduced experimental[34] UTF8SMTP command and later was superseded by RFC 6531 that introduced SMTPUTF8 command. These extensions provide support for multi-byte and non-ASCII characters in email addresses, such as those with diacritics and other language characters such as Greek and Chinese.[36]

Current support is limited, but there is strong interest in broad adoption of RFC 6531 and the related RFCs in countries like China that have a large user base where Latin (ASCII) is a foreign script.

Extensions
Like SMTP, ESMTP is a protocol used to transport Internet mail. It is used as both an inter-server transport protocol and (with restricted behavior enforced) a mail submission protocol.

The main identification feature for ESMTP clients is to open a transmission with the command EHLO (Extended HELLO), rather than HELO (Hello, the original RFC 821 standard). A server will respond with success (code 250), failure (code 550) or error (code 500, 501, 502, 504, or 421), depending on its configuration. An ESMTP server returns the code 250 OK in a multi-line reply with its domain and a list of keywords to indicate supported extensions. A RFC 821 compliant server returns error code 500, allowing ESMTP clients to try either HELO or QUIT.

Each service extension is defined in an approved format in subsequent RFCs and registered with the Internet Assigned Numbers Authority (IANA). The first definitions were the RFC 821 optional services: SEND, SOML (Send or Mail), SAML (Send and Mail), EXPN, HELP, and TURN. The format of additional SMTP verbs was set and for new parameters in MAIL and RCPT.

Some relatively common keywords (not all of them corresponding to commands) used today are:

8BITMIME – 8 bit data transmission, RFC 6152
ATRN – Authenticated TURN for On-Demand Mail Relay, RFC 2645
AUTH – Authenticated SMTP, RFC 4954
CHUNKING – Chunking, RFC 3030
DSN – Delivery status notification, RFC 3461 (See Variable envelope return path)
ETRN – Extended version of remote message queue starting command TURN, RFC 1985
HELP – Supply helpful information, RFC 821
PIPELINING – Command pipelining, RFC 2920
SIZE – Message size declaration, RFC 1870
STARTTLS – Transport Layer Security, RFC 3207 (2002)
SMTPUTF8 – Allow UTF-8 encoding in mailbox names and header fields, RFC 6531
UTF8SMTP – Allow UTF-8 encoding in mailbox names and header fields, RFC 5336 (deprecated[37])
The ESMTP format was restated in RFC 2821 (superseding RFC 821) and updated to the latest definition in RFC 5321 in 2008. Support for the EHLO command in servers became mandatory, and HELO designated a required fallback.

Non-standard, unregistered, service extensions can be used by bilateral agreement, these services are indicated by an EHLO message keyword starting with "X", and with any additional parameters or verbs similarly marked.

SMTP commands are case-insensitive. They are presented here in capitalized form for emphasis only. An SMTP server that requires a specific capitalization method is a violation of the standard.[30]

8BITMIME
At least the following servers advertise the 8BITMIME extension:

Apache James (since 2.3.0a1)[38]
Citadel (since 7.30)
Courier Mail Server
Gmail[39]
IceWarp
IIS SMTP Service
Kerio Connect
Lotus Domino
Microsoft Exchange Server (as of Exchange Server 2000)
Novell GroupWise
OpenSMTPD
Oracle Communications Messaging Server
Postfix
Sendmail (since 6.57)
ZMailer (since 2.95 940901)
The following servers can be configured to advertise 8BITMIME, but do not perform conversion of 8-bit data to 7-bit when connecting to non-8BITMIME relays:

Exim and qmail do not translate eight-bit messages to seven-bit when making an attempt to relay 8-bit data to non-8BITMIME peers, as is required by the RFC.[40] This does not cause problems in practice, since virtually all modern mail relays are 8-bit clean.[41]
Microsoft Exchange Server 2003 advertises 8BITMIME by default, but relaying to a non-8BITMIME peer results in a bounce. This is allowed by RFC 6152.[42]
SMTP-AUTH
Main article: SMTP Authentication
The SMTP-AUTH extension provides an access control mechanism. It consists of an authentication step through which the client effectively logs into the mail server during the process of sending mail. Servers that support SMTP-AUTH can usually be configured to require clients to use this extension, ensuring the true identity of the sender is known. The SMTP-AUTH extension is defined in RFC 4954.

SMTP-AUTH can be used to allow legitimate users to relay mail while denying relay service to unauthorized users, such as spammers. It does not necessarily guarantee the authenticity of either the SMTP envelope sender or the RFC 2822 "From:" header. For example, spoofing, in which one sender masquerades as someone else, is still possible with SMTP-AUTH unless the server is configured to limit message from-addresses to addresses this AUTHed user is authorized for.

The SMTP-AUTH extension also allows one mail server to indicate to another that the sender has been authenticated when relaying mail. In general this requires the recipient server to trust the sending server, meaning that this aspect of SMTP-AUTH is rarely used on the Internet.[43]

SMTPUTF8
Supporting servers include:

Postfix (version 3.0 and later)[44]
Exim (experimental as of the 4.86 release, quite mature in 4.96)
Sendmail (experimental support in 8.17.1)
Dovecot LMTP and IMAP/POP3 (experimental support in 2.4.0)
Momentum (versions 4.1[45] and 3.6.5, and later)
CommuniGate Pro as of version 6.2.2[46]
Courier-MTA as of version 1.0[47]
Halon as of version 4.0[48]
Microsoft Exchange Server as of protocol revision 14.0[49]
Haraka and other servers.[50]
Oracle Communications Messaging Server as of release 8.0.2.[51]
Security extensions
Mail delivery can occur both over plain text and encrypted connections, however the communicating parties might not know in advance of other party's ability to use secure channel.

STARTTLS or "Opportunistic TLS"
Main articles: Opportunistic TLS and Email encryption
The STARTTLS extensions enables supporting SMTP servers to notify connecting clients that it supports TLS encrypted communication and offers the opportunity for clients to upgrade their connection by sending the STARTTLS command. Servers supporting the extension do not inherently gain any security benefits from its implementation on its own, as upgrading to a TLS encrypted session is dependent on the connecting client deciding to exercise this option, hence the term opportunistic TLS.

STARTTLS is effective only against passive observation attacks, since the STARTTLS negotiation happens in plain text and an active attacker can trivially remove STARTTLS commands. This type of man-in-the-middle attack is sometimes referred to as STRIPTLS, where the encryption negotiation information sent from one end never reaches the other. In this scenario both parties take the invalid or unexpected responses as indication that the other does not properly support STARTTLS, defaulting to traditional plain-text mail transfer.[52] Note that STARTTLS is also defined for IMAP and POP3 in other RFCs, but these protocols serve different purposes: SMTP is used for communication between message transfer agents, while IMAP and POP3 are for end clients and message transfer agents.

In 2014 the Electronic Frontier Foundation began "STARTTLS Everywhere" project that, similarly to "HTTPS Everywhere" list, allowed relying parties to discover others supporting secure communication without prior communication. The project stopped accepting submissions on 29 April 2021, and EFF recommended switching to DANE and MTA-STS for discovering information on peers' TLS support.[53]

RFC 8314 officially declared plain text obsolete and recommend always using TLS for mail submission and access, adding ports with implicit TLS.

DANE for SMTP
RFC 7672 introduced the ability for DNS records to declare the encryption capabilities of a mail server. Utilising DNSSEC, mail server operators are able to publish a hash of their TLS certificate, thereby mitigating the possibility of unencrypted communications.[54]

Microsoft expects to enable full SMTP DANE support for Exchange Online customers by the end of 2024.[55]

SMTP MTA Strict Transport Security
A newer 2018 RFC 8461 called "SMTP MTA Strict Transport Security (MTA-STS)" aims to address the problem of active adversaries by defining a protocol for mail servers to declare their ability to use secure channels in specific files on the server and specific DNS TXT records. The relying party would regularly check existence of such record, and cache it for the amount of time specified in the record and never communicate over insecure channels until record expires.[52] Note that MTA-STS records apply only to SMTP traffic between mail servers while communications between a user's client and the mail server are protected by Transport Layer Security with SMTP/MSA, IMAP, POP3, or HTTPS in combination with an organizational or technical policy. Essentially, MTA-STS is a means to extend such a policy to third parties.

In April 2019 Google Mail announced support for MTA-STS.[56]

SMTP TLS Reporting
Protocols designed to securely deliver messages can fail due to misconfigurations or deliberate active interference, leading to undelivered messages or delivery over unencrypted or unauthenticated channels. RFC 8460 "SMTP TLS Reporting" describes a reporting mechanism and format for sharing statistics and specific information about potential failures with recipient domains. Recipient domains can then use this information to both detect potential attacks and diagnose unintentional misconfigurations.

In April 2019 Gmail announced support for SMTP TLS Reporting.[56]

Spoofing and spamming
Main articles: Anti-spam techniques and Email authentication
The original design of SMTP had no facility to authenticate senders, or check that servers were authorized to send on their behalf, with the result that email spoofing is possible, and commonly used in email spam and phishing.

Occasional proposals are made to modify SMTP extensively or replace it completely. One example of this is Internet Mail 2000, but neither it, nor any other has made much headway in the face of the network effect of the huge installed base of classic SMTP.

Instead, mail servers now use a range of techniques, such as stricter enforcement of standards such as RFC 5322,[57][58] DomainKeys Identified Mail, Sender Policy Framework and DMARC, DNSBLs and greylisting to reject or quarantine suspicious emails.[59]

Implementations
Main articles: List of mail server software and Comparison of mail servers
See also
Bounce address
CRAM-MD5 (a SASL mechanism for ESMTPA) RFC 2195
Email
Email encryption
DKIM
Ident
List of mail server software
List of SMTP server return codes
POP before SMTP / SMTP after POP
Internet Message Access Protocol Binary Content Extension RFC 3516
Sender Policy Framework (SPF)
Simple Authentication and Security Layer (SASL) RFC 4422
SMTP Authentication
Variable envelope return path
Comparison of email clients for information about SMTP support

---


Transport Layer Security
Communication protocol
OSI layer	Presentation layer / Application layer
IP number	TCP
RFCs	8446 (TLS 1.3)
5246 (TLS 1.2)
4346 (TLS 1.1)
2246 (TLS 1.0)
Website	tlswg.org
Transport Layer Security (TLS) is a cryptographic protocol designed to provide secured communications over a computer network, such as the Internet. The protocol is widely used in applications such as email, instant messaging, and voice over IP, but its use in securing HTTPS remains the most publicly visible.

The TLS protocol aims primarily to provide security, including privacy (confidentiality), integrity, and authenticity through the use of cryptography, such as the use of certificates, between two or more communicating computer applications. It runs in the presentation layer and is itself composed of two layers: the TLS record and the TLS handshake protocols.

The closely-related Datagram Transport Layer Security (DTLS) is a communications protocol that provides security to datagram-based applications. In technical writing, references to "(D)TLS" are often seen when it applies to both versions.[1]

TLS is a proposed Internet Engineering Task Force (IETF) standard, first defined in 1999, and the current version is TLS 1.3, defined in August 2018. TLS builds on the now-deprecated SSL (Secure Sockets Layer) specifications (1994, 1995, 1996) developed by Netscape Communications for adding the HTTPS protocol to their Netscape Navigator web browser.

Description
Client–server applications use the TLS protocol to communicate across a network in a way designed to prevent eavesdropping and tampering.

Since applications can communicate either with or without TLS (or SSL), it is necessary for the client to request that the server set up a TLS connection.[2] One of the main ways of achieving this is to use a different port number for TLS connections. Port 80 is typically used for unencrypted HTTP traffic while port 443 is the common port used for encrypted HTTPS traffic. Another mechanism is to make a protocol-specific STARTTLS request to the server to switch the connection to TLS – for example, when using some mail and news protocols.

Once the client and server have agreed to use TLS, they negotiate a stateful connection by using a handshaking procedure (see § TLS handshake).[3] The protocols use a handshake with an asymmetric cipher to establish not only cipher settings but also a session-specific shared key with which further communication is encrypted using a symmetric cipher. During this handshake, the client and server agree on various parameters used to establish the connection's security:

The handshake begins when a client connects to a TLS-enabled server requesting a secure connection and the client presents a list of supported cipher suites (ciphers and hash functions).
From this list, the server picks a cipher and hash function that it also supports and notifies the client of the decision.
The server usually then provides identification in the form of a digital certificate. The certificate contains the server name, the trusted certificate authority (CA) that vouches for the authenticity of the certificate, and the server's public encryption key.
The client confirms the validity of the certificate before proceeding.
To generate the session keys used for the secure connection, the client either:
encrypts a random number (PreMasterSecret) with the server's public key and sends the result to the server (which only the server should be able to decrypt with its private key); both parties then use the random number to generate a unique session key for subsequent encryption and decryption of data during the session, or
uses Diffie–Hellman key exchange (or its variant elliptic-curve DH) to securely generate a random and unique session key for encryption and decryption that has the additional property of forward secrecy: if the server's private key is disclosed in the future, it cannot be used to decrypt the current session, even if the session is intercepted and recorded by a third party.
This concludes the handshake and begins the secured connection, which is encrypted and decrypted with the session key until the connection closes. If any one of the above steps fails, then the TLS handshake fails and the connection is not created.

Older versions of TLS and SSL allowed for establishing sessions via a PreMasterSecret using the server's public and private key. These algorithms were eliminated in TLS1.3 when it mandated forward secrecy.[4]

TLS and SSL do not fit neatly into any single layer of the OSI model or the TCP/IP model.[5][6] TLS runs "on top of some reliable transport protocol (e.g., TCP)"[7]: §1 , which would imply that it is above the transport layer. It serves encryption to higher layers, which is normally the function of the presentation layer. However, applications generally use TLS as if it were a transport layer,[5][6] even though applications using TLS must actively control initiating TLS handshakes and handling of exchanged authentication certificates.[7]: §1 

When secured by TLS, connections between a client (e.g., a web browser) and a server (e.g., wikipedia.org) will have all of the following properties:[7]: §1 

The connection is private (or has confidentiality) because a symmetric-key algorithm is used to encrypt the data transmitted. The keys for this symmetric encryption are generated uniquely for each connection and are based on a shared secret that was negotiated at the start of the session. The server and client negotiate the details of which encryption algorithm and cryptographic keys to use before the first byte of data is transmitted (see below). The negotiation of a shared secret is both secure (the negotiated secret is unavailable to eavesdroppers and cannot be obtained, even by an attacker who places themselves in the middle of the connection) and reliable (no attacker can modify the communications during the negotiation without being detected).
The identity of the communicating parties can be authenticated using public-key cryptography. This authentication is required for the server and optional for the client.
The connection is reliable (or has integrity) because each message transmitted includes a message integrity check using a message authentication code to prevent undetected loss or alteration of the data during transmission.
TLS supports many different methods for exchanging keys, encrypting data, and authenticating message integrity. As a result, secure configuration of TLS involves many configurable parameters, and not all choices provide all of the privacy-related properties described in the list above (see the tables below § Key exchange, § Cipher security, and § Data integrity).

Attempts have been made to subvert aspects of the communications security that TLS seeks to provide, and the protocol has been revised several times to address these security threats. Developers of web browsers have repeatedly revised their products to defend against potential security weaknesses after these were discovered (see TLS/SSL support history of web browsers).

Datagram Transport Layer Security
Datagram Transport Layer Security, abbreviated DTLS, is a related communications protocol providing security to datagram-based applications by allowing them to communicate in a way designed[8][9] to prevent eavesdropping, tampering, or message forgery. The DTLS protocol is based on the stream-oriented Transport Layer Security (TLS) protocol and is intended to provide similar security guarantees. However, unlike TLS, it can be used with most datagram oriented protocols including User Datagram Protocol (UDP), Datagram Congestion Control Protocol (DCCP), Control And Provisioning of Wireless Access Points (CAPWAP), Stream Control Transmission Protocol (SCTP) encapsulation, and Secure Real-time Transport Protocol (SRTP).

As the DTLS protocol datagram preserves the semantics of the underlying transport, the application does not suffer from the delays associated with stream protocols. However, the application has to deal with packet reordering, loss of datagram and data larger than the size of a datagram network packet. Because DTLS uses UDP or SCTP rather than TCP, it avoids the TCP meltdown problem,[10][11] when being used to create a VPN tunnel.

The original 2006 release of DTLS version 1.0 was not a standalone document. It was given as a series of deltas to TLS 1.1.[8]: §4  Similarly the follow-up 2012 release of DTLS is a delta to TLS 1.2. It was given the version number of DTLS 1.2 to match its TLS version. Lastly, the 2022 DTLS 1.3 is a delta to TLS 1.3. Like the two previous versions, DTLS 1.3 is intended to provide "equivalent security guarantees [to TLS 1.3] with the exception of order protection/non-replayability".[12]

Many VPN clients including Cisco AnyConnect[13] & InterCloud Fabric,[14] OpenConnect,[15] ZScaler tunnel,[16] F5 Networks Edge VPN Client,[17] and Citrix Systems NetScaler[18] use DTLS to secure UDP traffic. In addition all modern web browsers support DTLS-SRTP[19] for WebRTC.

Post-quantum key exchange
To protect against "harvest now, decrypt later" attacks by future quantum computers, hybrid post-quantum key agreement schemes have been deployed for TLS 1.3. These combine a classical ECDHE exchange with ML-KEM, the lattice-based key encapsulation mechanism standardized by NIST as FIPS 203 in August 2024.[20] The IETF TLS working group defines the hybrid groups X25519MLKEM768, SecP256r1MLKEM768 and SecP384r1MLKEM1024 in an Internet-Draft,[21] alongside a general framework for hybrid key exchange in TLS 1.3[22] and a specification for standalone (non-hybrid) ML-KEM key agreement.[23]

X25519MLKEM768 (and its predecessor X25519Kyber768) has been enabled by default in Google Chrome since version 131 (November 2024)[24] and Firefox since version 132,[25] and is supported by server-side implementations including OpenSSL 3.5,[26] BoringSSL and Cloudflare.[27]

History and development
SSL and TLS protocols
Protocol	Published	Status
SSL 1.0	Unpublished	Unpublished
SSL 2.0	1995	Deprecated in 2011[28]
SSL 3.0	1996	Deprecated in 2015[29]
TLS 1.0	1999	Deprecated in 2021[30][31][32][33]
TLS 1.1	2006	Deprecated in 2021[30][31][32][33]
TLS 1.2	2008	In use since 2008[34][35]
TLS 1.3	2018	In use since 2018[35][36]
Unsupported
Supported
Latest version
Early research projects
Secure Data Network System
In August 1986, the National Security Agency, the National Bureau of Standards, the Defense Communications Agency launched a project, called the Secure Data Network System (SDNS), with the intent of designing the next generation of secure computer communications network and product specifications to be implemented for applications on public and private internets. It was intended to complement the rapidly emerging new OSI internet standards moving forward both in the U.S. government's GOSIP Profiles and in the huge ITU-ISO JTC1 internet effort internationally.[37]

As part of the project, researchers designed a protocol called SP4 (security protocol in layer 4 of the OSI system). This was later renamed the Transport Layer Security Protocol (TLSP) and subsequently published in 1995 as international standard ITU-T X.274|ISO/IEC 10736:1995.[38] Despite the name similarity, this is distinct from today's TLS.

Secure Network Programming (SNP)
Other efforts towards transport layer security included the Secure Network Programming (SNP) application programming interface (API), which in 1993 explored the approach of having a secure transport layer API closely resembling Berkeley sockets, to facilitate retrofitting pre-existing network applications with security measures. SNP was published and presented in the 1994 USENIX Summer Technical Conference.[39][40] The SNP project was funded by a grant from NSA to Professor Simon Lam at UT-Austin in 1991.[41] Secure Network Programming won the 2004 ACM Software System Award.[42][43] Simon Lam was inducted into the Internet Hall of Fame for "inventing secure sockets in 1991 and implementing the first secure sockets layer, named SNP, in 1993."[44][45]

SSL 1.0, 2.0, and 3.0
"SSL 1" redirects here. For the enzyme, see Presqualene diphosphate synthase.
Netscape developed the original SSL protocols, and Taher Elgamal, chief scientist at Netscape Communications from 1995 to 1998, has been described as the "father of SSL".[46][47][48][49] SSL version 1.0 was never publicly released because of serious security flaws in the protocol. Version 2.0, after being released in February 1995 was quickly found to contain a number of security and usability flaws. It used the same cryptographic keys for message authentication and encryption. It had a weak MAC construction that used the MD5 hash function with a secret prefix, making it vulnerable to length extension attacks. It also provided no protection for either the opening handshake or an explicit message close, both of which meant man-in-the-middle attacks could go undetected. Moreover, SSL 2.0 assumed a single service and a fixed domain certificate, conflicting with the widely used feature of virtual hosting in Web servers, so most websites were effectively impaired from using SSL.

These flaws necessitated the complete redesign of the protocol to SSL version 3.0.[50][48] Released in 1996, it was produced by Paul Kocher working with Netscape engineers Phil Karlton and Alan Freier, with a reference implementation by Christopher Allen and Tim Dierks of Certicom. Newer versions of SSL/TLS are based on SSL 3.0. The 1996 draft of SSL 3.0 was published by IETF as a historical document in RFC 6101.

SSL 2.0 was deprecated in 2011 by RFC 6176. In 2014, SSL 3.0 was found to be vulnerable to the POODLE attack that affects all block ciphers in SSL; RC4, the only non-block cipher supported by SSL 3.0, is also feasibly broken as used in SSL 3.0.[51] SSL 3.0 was deprecated in June 2015 by RFC 7568.

TLS 1.0
TLS 1.0 was first defined in RFC 2246 in January 1999 as an upgrade of SSL Version 3.0, and written by Christopher Allen and Tim Dierks of Certicom. As stated in the RFC, "the differences between this protocol and SSL 3.0 are not dramatic, but they are significant enough to preclude interoperability between TLS 1.0 and SSL 3.0". Tim Dierks later wrote that these changes, and the renaming from "SSL" to "TLS", were a face-saving gesture to Microsoft, "so it wouldn't look [like] the IETF was just rubberstamping Netscape's protocol".[52]

The PCI Council suggested that organizations migrate from TLS 1.0 to TLS 1.1 or higher before June 30, 2018.[53][54] In October 2018, Apple, Google, Microsoft, and Mozilla jointly announced they would deprecate TLS 1.0 and 1.1 in March 2020.[31] TLS 1.0 and 1.1 were formally deprecated in RFC 8996 in March 2021.

TLS 1.1
TLS 1.1 was defined in RFC 4346 in April 2006.[55] It is an update from TLS version 1.0. Significant differences in this version include:

Added protection against cipher-block chaining (CBC) attacks.
The implicit initialization vector (IV) was replaced with an explicit IV.
Change in handling of padding errors.
Support for IANA registration of parameters.[56]
Support for TLS versions 1.0 and 1.1 was widely deprecated by web sites around 2020,[57] disabling access to Firefox versions before 24 and Chromium-based browsers before 29,[58] though third-party fixes can be applied to Netscape Navigator and older versions of Firefox to add TLS 1.2 support.[59]

TLS 1.2
TLS 1.2 was defined in RFC 5246 in August 2008.[34] It is based on the earlier TLS 1.1 specification. Major differences include:

The MD5 and SHA-1 combination in the pseudorandom function (PRF) was replaced with SHA-256, with an option to use cipher suite specified PRFs.
The MD5 and SHA-1 combination in the finished message hash was replaced with SHA-256, with an option to use cipher suite specific hash algorithms. However, the size of the hash in the finished message must still be at least 96 bits.[34]: §7.4.9 
The MD5 and SHA-1 combination in the digitally signed element was replaced with a single hash negotiated during handshake, which defaults to SHA-1.
Enhancement in the client's and server's ability to specify which hashes and signature algorithms they accept.
Expansion of support for authenticated encryption ciphers, used mainly for Galois/Counter Mode (GCM) and CCM mode of Advanced Encryption Standard (AES) encryption.
TLS Extensions definition and AES cipher suites were added.[56]
All TLS versions were further refined in RFC 6176 in March 2011, removing their backward compatibility with SSL such that TLS sessions never negotiate the use of Secure Sockets Layer (SSL) version 2.0.

As of July 2026, there is no formal date for TLS 1.2 to be deprecated which allows for legacy clients. However the use of MD5 and SHA1 hash methods as well as Diffie-Hellman (DH) over a finite field and RSA key exchanges have been deprecated. (RFC 9155, 10015)

TLS 1.3
TLS 1.3 was defined in RFC 8446 in August 2018.[7] and updated with RFC 9846 in July 2026. It is based on the earlier TLS 1.2 specification. Major differences from TLS 1.2 include:[4]

Separating key agreement and authentication algorithms from the cipher suites[56][7]: §11 
Removing support for weak and less-used named elliptic curves
Removing support for MD5 and SHA-224 cryptographic hash functions
Requiring digital signatures even when a previous configuration is used
Integrating HKDF and the semi-ephemeral DH proposal
Replacing resumption with PSK and tickets
Supporting 1-RTT handshakes and initial support for 0-RTT
Mandating perfect forward secrecy, by means of using ephemeral keys during the (EC)DH key agreement
Dropping support for many insecure or obsolete features including compression, renegotiation, non-AEAD ciphers, null ciphers,[60] non-PFS key exchange (among which are static RSA and static DH key exchanges), custom DHE groups, EC point format negotiation, Change Cipher Spec protocol, Hello message UNIX time, and the length field AD input to AEAD ciphers
Prohibiting SSL or RC4 negotiation for backwards compatibility
Integrating use of session hash
Deprecating use of the record layer version number and freezing the number for improved backwards compatibility
Moving some security-related algorithm details from an appendix to the specification and relegating ClientKeyShare to an appendix
Adding the ChaCha20 stream cipher with the Poly1305 message authentication code
Adding the Ed25519 and Ed448 digital signature algorithms
Adding the x25519 and x448 key exchange protocols
Adding support for sending multiple OCSP responses
Encrypting all handshake messages after the ServerHello, including server certificate
Network Security Services (NSS), the cryptography library developed by Mozilla and used by its web browser Firefox, enabled TLS 1.3 by default in February 2017.[61] TLS 1.3 support was subsequently added — but due to compatibility issues for a small number of users, not automatically enabled[62] — to Firefox 52.0, which was released in March 2017. TLS 1.3 was enabled by default in May 2018 with the release of Firefox 60.0.[63]

Google Chrome set TLS 1.3 as the default version for a short time in 2017. It then removed it as the default, due to incompatible middleboxes such as Blue Coat web proxies.[64]

The intolerance of the new version of TLS was protocol ossification; middleboxes had ossified the protocol's version parameter. As a result, version 1.3 mimics the wire image of version 1.2. This change occurred very late in the design process, only having been discovered during browser deployment.[65] The discovery of this intolerance also led to the prior version negotiation strategy, where the highest matching version was picked, being abandoned due to unworkable levels of ossification.[66] 'Greasing' an extension point, where one protocol participant claims support for non-existent extensions to ensure that unrecognised-but-actually-existent extensions are tolerated and so to resist ossification, was originally designed for TLS, but it has since been adopted elsewhere.[66]

During the IETF 100 Hackathon, which took place in Singapore in 2017, the TLS Group worked on adapting open-source applications to use TLS 1.3.[67][68] The TLS group was made up of individuals from Japan, United Kingdom, and Mauritius via the cyberstorm.mu team.[68] This work was continued in the IETF 101 Hackathon in London,[69] and the IETF 102 Hackathon in Montreal.[70]

wolfSSL enabled the use of TLS 1.3 as of version 3.11.1, released in May 2017.[71] As the first commercial TLS 1.3 implementation, wolfSSL 3.11.1 supported Draft 18 and now supports Draft 28,[72] the final version, as well as many older versions. A series of blogs were published on the performance difference between TLS 1.2 and 1.3.[73]

In September 2018, the popular OpenSSL project released version 1.1.1 of its library, in which support for TLS 1.3 was "the headline new feature".[74]

Support for TLS 1.3 was added to Secure Channel (schannel) for the GA releases of Windows 11 and Windows Server 2022.[75]

Enterprise Transport Security
The Electronic Frontier Foundation praised TLS 1.3 and expressed concern about the variant protocol Enterprise Transport Security (ETS) that intentionally disables important security measures in TLS 1.3.[76] Originally called Enterprise TLS (eTLS), ETS is a published standard known as the 'ETSI TS103523-3', "Middlebox Security Protocol, Part3: Enterprise Transport Security". It is intended for use entirely within proprietary networks such as banking systems. ETS does not support forward secrecy so as to allow third-party organizations connected to the proprietary networks to be able to use their private key to monitor network traffic for the detection of malware and to make it easier to conduct audits.[77][78] Despite the claimed benefits, the EFF warned that the loss of forward secrecy could make it easier for data to be exposed along with saying that there are better ways to analyze traffic.[76]

Digital certificates
Main article: Public key certificate

Example of a website with digital certificate
A digital certificate certifies the ownership of a public key by the named subject of the certificate, and indicates certain expected usages of that key. This allows others (relying parties) to rely upon signatures or on assertions made by the private key that corresponds to the certified public key. Keystores and trust stores can be in various formats, such as .pem, .crt, .pfx/.p12 and .jks.

Certificate authorities
Main article: Certificate authority
TLS typically relies on a set of trusted third-party certificate authorities to establish the authenticity of certificates. Trust is usually anchored in a list of certificates distributed with user agent software,[79] and can be modified by the relying party.

According to Netcraft, who monitors active TLS certificates, the market-leading certificate authority (CA) has been Symantec since the beginning of their survey (or VeriSign before the authentication services business unit was purchased by Symantec). As of 2015, Symantec accounted for just under a third of all certificates and 44% of the valid certificates used by the 1 million busiest websites, as counted by Netcraft.[80] In 2017, Symantec sold its TLS/SSL business to DigiCert.[81] In an updated report, it was shown that IdenTrust, DigiCert, and Sectigo are the top 3 certificate authorities in terms of market share since May 2019.[82]

As a consequence of choosing X.509 certificates, certificate authorities and a public key infrastructure are necessary to verify the relation between a certificate and its owner, as well as to generate, sign, and administer the validity of certificates. While this can be more convenient than verifying the identities via a web of trust, the 2013 mass surveillance disclosures made it more widely known that certificate authorities are a weak point from a security standpoint, allowing man-in-the-middle attacks (MITM) if the certificate authority cooperates (or is compromised).[83][84]

On April 11, 2025, the CA/Browser Forum approved a ballot that will require all public TLS certificate lifespans to gradually reduce to 47 days by 2029.[85] The ballot was proposed by Apple.[86]

Algorithms
See also: Cipher suite
Key exchange or key agreement
Before a client and server can begin to exchange information protected by TLS, they must securely exchange or agree upon an encryption key and a cipher to use when encrypting data (see § Cipher). Among the methods used for key exchange/agreement are: public and private keys generated with RSA (denoted TLS_RSA in the TLS handshake protocol), Diffie–Hellman (TLS_DH), ephemeral Diffie–Hellman (TLS_DHE), elliptic-curve Diffie–Hellman (TLS_ECDH), ephemeral elliptic-curve Diffie–Hellman (TLS_ECDHE), anonymous Diffie–Hellman (TLS_DH_anon),[34] pre-shared key (TLS_PSK)[87] and Secure Remote Password (TLS_SRP).[88]

The TLS_DH_anon and TLS_ECDH_anon key agreement methods do not authenticate the server or the user and hence are rarely used because those are vulnerable to man-in-the-middle attacks. Only TLS_DHE and TLS_ECDHE provide forward secrecy.

Public key certificates used during exchange/agreement also vary in the size of the public/private encryption keys used during the exchange and hence the robustness of the security provided. In July 2013, Google announced that it would no longer use 1024-bit public keys and would switch instead to 2048-bit keys to increase the security of the TLS encryption it provides to its users because the encryption strength is directly related to the key size.[89][90]


Key exchange/agreement and authentication
Algorithm	SSL 2.0	SSL 3.0	TLS 1.0	TLS 1.1	TLS 1.2	TLS 1.3	Status
RSA	Yes	Yes	Yes	Yes	Yes	No	Defined for TLS 1.2 in RFCs
DH-RSA	No	Yes	Yes	Yes	Yes	No
DHE-RSA (forward secrecy)	No	Yes	Yes	Yes	Yes	Yes
ECDH-RSA	No	No	Yes	Yes	Yes	No
ECDHE-RSA (forward secrecy)	No	No	Yes	Yes	Yes	Yes
DH-DSS	No	Yes	Yes	Yes	Yes	No
DHE-DSS (forward secrecy)	No	Yes	Yes	Yes	Yes	No[91]
DHE-ECDSA (forward secrecy)	No	No	No	No	No	Yes
ECDH-ECDSA	No	No	Yes	Yes	Yes	No
ECDHE-ECDSA (forward secrecy)	No	No	Yes	Yes	Yes	Yes
DHE-EdDSA (forward secrecy)	No	No	No	No	No	Yes
ECDH-EdDSA	No	No	Yes	Yes	Yes	No
ECDHE-EdDSA (forward secrecy)[92]	No	No	Yes	Yes	Yes	Yes
PSK	No	No	Yes	Yes	Yes	Yes
RSA-PSK	No	No	Yes	Yes	Yes	No
DHE-PSK (forward secrecy)	No	No	Yes	Yes	Yes	Yes
ECDHE-PSK (forward secrecy)	No	No	Yes	Yes	Yes	Yes
SRP	No	No	Yes	Yes	Yes	No
SRP-DSS	No	No	Yes	Yes	Yes	No
SRP-RSA	No	No	Yes	Yes	Yes	No
Kerberos	No	No	Yes	Yes	Yes	?
DH-ANON (insecure)	No	Yes	Yes	Yes	Yes	No
ECDH-ANON (insecure)	No	No	Yes	Yes	Yes	No
GOST R 34.10-2012[93]	No	No	No	No	Yes	Yes	Defined for TLS 1.2[94] and for TLS 1.3.[95]
Cipher
See also: Cipher suite, Block cipher, and Cipher security summary

Cipher security against publicly known feasible attacks
Cipher	Protocol version	Status
Type	Algorithm	Nominal strength (bits)	SSL 2.0	SSL 3.0
[n 1][n 2][n 3][n 4]	TLS 1.0
[n 1][n 3]	TLS 1.1
[n 1]	TLS 1.2
[n 1]	TLS 1.3
Block cipher with mode of operation	AES GCM[96][97][n 5]	256, 128	—N/a	—N/a	—N/a	—N/a	Secure	Secure	Defined for TLS 1.2 in RFCs
AES CCM[98][99][n 5]	—N/a	—N/a	—N/a	—N/a	Secure	Secure
AES CBC[n 6]	—N/a	Insecure	Depends on mitigations	Depends on mitigations	Depends on mitigations	—N/a
Camellia GCM[100][n 5]	256, 128	—N/a	—N/a	—N/a	—N/a	Secure	—N/a
Camellia CBC[101][100][n 6]	—N/a	Insecure	Depends on mitigations	Depends on mitigations	Depends on mitigations	—N/a
ARIA GCM[102][n 5]	256, 128	—N/a	—N/a	—N/a	—N/a	Secure	—N/a
ARIA CBC[102][n 6]	—N/a	—N/a	Depends on mitigations	Depends on mitigations	Depends on mitigations	—N/a
SEED CBC[103][n 6]	128	—N/a	Insecure	Depends on mitigations	Depends on mitigations	Depends on mitigations	—N/a
3DES EDE CBC[n 6][n 7]	112[n 8]	Insecure	Insecure	Insecure	Insecure	Insecure	—N/a
GOST R 34.12-2015 Magma CTR[93][n 7]	256	—N/a	—N/a	Insecure	Insecure	Insecure	—N/a	Defined in RFC 4357, 9189
GOST R 34.12-2015 Kuznyechik CTR[93]	256	—N/a	—N/a	—N/a	—N/a	Secure	—N/a	Defined in RFC 9189
GOST R 34.12-2015 Magma MGM[93][n 5][n 7]	256	—N/a	—N/a	—N/a	—N/a	—N/a	Insecure	Defined in RFC 9367
GOST R 34.12-2015 Kuznyechik MGM[93][n 5]	256	—N/a	—N/a	—N/a	—N/a	—N/a	Secure	Defined in RFC 9367
IDEA CBC[n 6][n 7][n 9]	128	Insecure	Insecure	Insecure	Insecure	—N/a	—N/a	Removed from TLS 1.2
DES CBC[n 6][n 7][n 9]	56	Insecure	Insecure	Insecure	Insecure	—N/a	—N/a
40[n 10]	Insecure	Insecure	Insecure	—N/a	—N/a	—N/a	Forbidden in TLS 1.1 and later
RC2 CBC[n 6][n 7]	40[n 10]	Insecure	Insecure	Insecure	—N/a	—N/a	—N/a
Stream cipher	ChaCha20-Poly1305[108][n 5]	256	—N/a	—N/a	—N/a	—N/a	Secure	Secure	Defined for TLS 1.2 in RFCs
RC4[n 11]	128	Insecure	Insecure	Insecure	Insecure	Insecure	—N/a	Prohibited in all versions of TLS[109]
40[n 10]	Insecure	Insecure	Insecure	—N/a	—N/a	—N/a
None	Null[n 12]	–	Insecure	Insecure	Insecure	Insecure	Insecure	—N/a	Defined for TLS 1.2 in RFCs
Notes

 RFC 5746 must be implemented to fix a renegotiation flaw that would otherwise break this protocol.
 If libraries implement fixes listed in RFC 5746, this violates the SSL 3.0 specification, which the IETF cannot change unlike TLS. Most current libraries implement the fix and disregard the violation that this causes.
 The BEAST attack breaks all block ciphers (CBC ciphers) used in SSL 3.0 and TLS 1.0 unless mitigated by the client or the server. See § Web browsers.
 The POODLE attack breaks all block ciphers (CBC ciphers) used in SSL 3.0 unless mitigated by the client or the server. See § Web browsers.
 AEAD ciphers (such as GCM and CCM) can only be used in TLS 1.2 or later.
 CBC ciphers can be attacked with the Lucky Thirteen attack if the library is not written carefully to eliminate timing side channels.
 The Sweet32 attack breaks block ciphers with a block size of 64 bits.[104]
 Although the key length of 3DES is 168 bits, effective security strength of 3DES is only 112 bits,[105] which is below the recommended minimum of 128 bits.[106]
 IDEA and DES have been removed from TLS 1.2.[107]
 40-bit strength cipher suites were intentionally designed with reduced key lengths to comply with since-rescinded US regulations forbidding the export of cryptographic software containing certain strong encryption algorithms (see Export of cryptography from the United States). These weak suites are forbidden in TLS 1.1 and later.
 Use of RC4 in all versions of TLS is prohibited because RC4 attacks weaken or break RC4 used in SSL/TLS.
 Authentication only, no encryption.
Data integrity
A message authentication code (MAC) is used for data integrity. HMAC is used for CBC mode of block ciphers. Authenticated encryption (AEAD) such as GCM and CCM mode uses AEAD-integrated MAC and does not use HMAC.[7]: §8.4  HMAC-based PRF, or HKDF is used for TLS handshake.


Data integrity
Algorithm	SSL 2.0	SSL 3.0	TLS 1.0	TLS 1.1	TLS 1.2	TLS 1.3	Status
HMAC-MD5	Yes	Yes	Yes	Yes	Yes	No	Defined for TLS 1.2 in RFCs
HMAC-SHA1	No	Yes	Yes	Yes	Yes	No
HMAC-SHA256/384	No	No	No	No	Yes	No
AEAD	No	No	No	No	Yes	Yes
GOST 28147-89 IMIT[93]	No	No	No	No	Yes	No	Defined for TLS 1.2 in RFC 9189.
GOST R 34.12-2015 AEAD[93]	No	No	No	No	No	Yes	Defined for TLS 1.3 in RFC 9367.
Applications and adoption
In applications design, TLS is usually implemented on top of Transport Layer protocols, encrypting all of the protocol-related data of protocols such as HTTP, FTP, SMTP, NNTP and XMPP.

Historically, TLS has been used primarily with reliable transport protocols such as the Transmission Control Protocol (TCP). However, it has also been implemented with datagram-oriented transport protocols, such as the User Datagram Protocol (UDP) and the Datagram Congestion Control Protocol (DCCP), usage of which has been standardized independently using the term Datagram Transport Layer Security (DTLS).

Websites
A primary use of TLS is to secure World Wide Web traffic between a website and a web browser encoded with the HTTP protocol. This use of TLS to secure HTTP traffic constitutes the HTTPS protocol.[110]

Selected Websites' protocol support (September 2025)
Protocol
version	selected[111] Web­sites'
support	Security[111][112]
SSL 2.0	0.1%	Insecure
SSL 3.0	1.0%	Insecure[113]
TLS 1.0	23.5%	Deprecated[31][32][33]
TLS 1.1	25.2%	Deprecated[31][32][33]
TLS 1.2	100%	Depends on cipher[n 1] and client mitigations[n 2]
TLS 1.3	75.3%	Secure

---

The Session Announcement Protocol (SAP) is an experimental protocol for advertising multicast session information. SAP typically uses Session Description Protocol (SDP) as the format for Real-time Transport Protocol (RTP) session descriptions. Announcement data is sent using IP multicast and the User Datagram Protocol (UDP).

Under SAP, senders periodically transmit SDP descriptions to a well-known multicast address and port number (9875).[1] A listening application constructs a guide of all advertised multicast sessions.

SAP was published by the IETF as RFC 2974.[2]

Announcement interval
The announcement interval is cooperatively modulated such that all SAP announcements in the multicast delivery scope, by default, consume 4000 bits per second. Regardless, the maximum announce interval is 300 seconds (5 minutes). Announcements automatically expire after 10 times the announcement interval or one hour, whichever is greater. Announcements may also be explicitly withdrawn by the original issuer.

Authentication, encryption and compression
SAP features separate methods for authenticating and encrypting announcements. Use of encryption is not recommended. Authentication prevents unauthorized modification and other DoS attacks. Authentication is optional. Two authentication schemes are supported:

Pretty Good Privacy as defined in RFC 2440
Cryptographic Message Syntax as defined in RFC 5652
The message body may optionally be compressed using the zlib format as defined in RFC 1950.

Applications and implementations
VLC media player monitors SAP announcements and presents the user a list of available streams.[3]

SAP is one of the optional discovery and connection management techniques described in the AES67 audio-over-Ethernet interoperability standard.[4]

---


Session Initiation Protocol
Communication protocol

Example of a simple SIP call establishment
Abbreviation	SIP
Purpose	Internet telephony
Introduction	March 1999; 27 years ago
OSI layer	Application layer (Layer 7)
Ports	5060, 5061
RFCs	2543, 3261
Internet protocol suite
Application layer
BGPDHCP (v6)DNSFTPHTTP (HTTP/3)HTTPSIMAPJMAPIPPIRCLDAPMGCPMQTTNNTPNTPOSPFPOPPTPONC/RPCRTPRTSPRIPSIPSMTPSNMPSSHTelnetTLS/SSLXMPPmore...
Transport layer
TCPUDPDCCPSCTPRSVPQUICmore...
Internet layer
IP v4v6ICMP (v6)NDPECNL4SIGMPIPsecmore...
Link layer
ARPTunnelsPPPMACmore...
vte
The Session Initiation Protocol (SIP) is a signaling protocol used for initiating, maintaining, modifying, and terminating communication sessions that involve multimedia elements such as voice, video, and messaging.[1] It operates at the application layer of the Internet protocol suite and is widely used in Internet telephony, private IP-based telephone systems, and mobile communication over LTE networks through VoLTE.[2]

The protocol defines the specific format of messages exchanged and the sequence of communications for cooperation of the participants. SIP is a text-based protocol, incorporating many elements of the Hypertext Transfer Protocol (HTTP) and the Simple Mail Transfer Protocol (SMTP).[3] A call established with SIP may consist of multiple media streams, but no separate streams are required for applications, such as text messaging, that exchange data as payload in the SIP message.

SIP works in conjunction with several other protocols that specify and carry the session media. Most commonly, media type and parameter negotiation and media setup are performed with the Session Description Protocol (SDP), which is carried as payload in SIP messages. SIP is designed to be independent of the underlying transport layer protocol and can be used with the User Datagram Protocol (UDP), the Transmission Control Protocol (TCP), and the Stream Control Transmission Protocol (SCTP). For secure transmissions of SIP messages over insecure network links, the protocol may be encrypted with Transport Layer Security (TLS). For the transmission of media streams (voice, video) the SDP payload carried in SIP messages typically employs the Real-time Transport Protocol (RTP) or the Secure Real-time Transport Protocol (SRTP).

History
SIP was originally designed by Mark Handley, Henning Schulzrinne, Eve Schooler and Jonathan Rosenberg in 1996 to facilitate establishing multicast multimedia sessions on the Mbone. The protocol was standardized as RFC 2543 in 1999. In November 2000, SIP was accepted as a 3GPP signaling protocol and permanent element of the IP Multimedia Subsystem (IMS) architecture for IP-based streaming multimedia services in cellular networks. In June 2002 the specification was revised in RFC 3261[4] and various extensions and clarifications have been published since.[5]

SIP was designed to provide a signaling and call setup protocol for IP-based communications supporting the call processing functions and features present in the public switched telephone network (PSTN) with a vision of supporting new multimedia applications. It has been extended for video conferencing, streaming media distribution, instant messaging, presence information, file transfer, Internet fax and online games.[1][6][7]

SIP is distinguished by its proponents for having roots in the Internet community rather than in the telecommunications industry. SIP has been standardized primarily by the Internet Engineering Task Force (IETF), while other protocols, such as H.323, have traditionally been associated with the International Telecommunication Union (ITU).[3]

Protocol operation

An example of a SIP message exchange between two users, Alice and Bob, to establish and end a direct media session.
SIP is only involved in the signaling operations of a media communication session and is primarily used to set up and terminate voice or video calls. SIP can be used to establish two-party (unicast) or multiparty (multicast) sessions. It also allows modification of existing calls. The modification can involve changing addresses or ports, inviting more participants, and adding or deleting media streams. SIP has also found applications in messaging applications, such as instant messaging, and event subscription and notification.[8]

SIP works in conjunction with several other protocols that specify the media format and coding and that carry the media once the call is set up. For call setup, the body of a SIP message contains a Session Description Protocol (SDP) data unit, which specifies the media format, codec and media communication protocol. Voice and video media streams are typically carried between the terminals using the Real-time Transport Protocol (RTP) or Secure Real-time Transport Protocol (SRTP).[8][9]

Every resource of a SIP network, such as user agents, call routers, and voicemail boxes, are identified by a Uniform Resource Identifier (URI). The syntax of the URI follows the general standard syntax also used in Web services and e-mail.[10] The URI scheme used for SIP is sip and a typical SIP URI has the form sip:username@domainname or sip:username@hostport, where domainname requires DNS SRV records to locate the servers for SIP domain while hostport can be an IP address or a fully qualified domain name of the host and port. If secure transmission is required, the scheme sips is used.[11][12]

SIP employs design elements similar to the HTTP request and response transaction model.[13] Each transaction consists of a client request that invokes a particular method or function on the server and at least one response. SIP reuses most of the header fields, encoding rules and status codes of HTTP, providing a readable text-based format.

SIP can be carried by several transport layer protocols including Transmission Control Protocol (TCP), User Datagram Protocol (UDP), and Stream Control Transmission Protocol (SCTP).[14][15] SIP clients typically use TCP or UDP on port numbers 5060 or 5061 for SIP traffic to servers and other endpoints. Port 5060 is commonly used for non-encrypted signaling traffic whereas port 5061 is typically used for traffic encrypted with Transport Layer Security (TLS).[16]

SIP-based telephony networks often implement call processing features of Signaling System 7 (SS7), for which special SIP protocol extensions exist, although the two protocols themselves are very different. SS7 is a centralized protocol, characterized by a complex central network architecture and dumb endpoints (traditional telephone handsets). SIP is a client-server protocol of equipotent peers. SIP features are implemented in the communicating endpoints, while the traditional SS7 architecture is in use only between switching centers.[17]

Network elements
The network elements that use the Session Initiation Protocol for communication are called SIP user agents. Each user agent (UA) performs the function of a user agent client (UAC) when it is requesting a service function, and that of a user agent server (UAS) when responding to a request. Thus, any two SIP endpoints may in principle operate without any intervening SIP infrastructure. However, for network operational reasons, for provisioning public services to users, and for directory services, SIP defines several specific types of network server elements. Each of these service elements also communicates within the client-server model implemented in user agent clients and servers.[18]

User agent
A user agent is a logical network endpoint that sends or receives SIP messages and manages SIP sessions. User agents have client and server components. The user agent client (UAC) sends SIP requests. The user agent server (UAS) receives requests and returns a SIP response. Unlike other network protocols that fix the roles of client and server, e.g., in HTTP, in which a web browser only acts as a client, and never as a server, SIP requires both peers to implement both roles. The roles of UAC and UAS only last for the duration of a SIP transaction.[6]

A SIP phone is an IP phone that implements client and server functions of a SIP user agent and provides the traditional call functions of a telephone, such as dial, answer, reject, call hold, and call transfer.[19][20] SIP phones may be implemented as a hardware device or as a softphone. As vendors increasingly implement SIP as a standard telephony platform, the distinction between hardware-based and software-based SIP phones is blurred and SIP elements are implemented in the basic firmware functions of many IP-capable communications devices such as smartphones.[21]

In SIP, as in HTTP, the user agent may identify itself using a message header field (User-Agent), containing a text description of the software, hardware, or the product name. The user agent field is sent in request messages, which means that the receiving SIP server can evaluate this information to perform device-specific configuration or feature activation. Operators of SIP network elements sometimes store this information in customer account portals,[22] where it can be useful in diagnosing SIP compatibility problems or in the display of service status.

Proxy server
A proxy server is a network server with UAC and UAS components that functions as an intermediary entity for the purpose of performing requests on behalf of other network elements. A proxy server primarily plays the role of call routing; it sends SIP requests to another entity closer to the destination. Proxies are also useful for enforcing policy, such as for determining whether a user is allowed to make a call. A proxy interprets, and, if necessary, rewrites specific parts of a request message before forwarding it.

SIP proxy servers that route messages to more than one destination are called forking proxies. The forking of a SIP request establishes multiple dialogs from the single request. Thus, a call may be answered from one of multiple SIP endpoints. For identification of multiple dialogs, each dialog has an identifier with contributions from both endpoints.

Redirect server
A redirect server is a user agent server that generates 3xx (redirection) responses to requests it receives, directing the client to contact an alternate set of URIs. A redirect server allows proxy servers to direct SIP session invitations to external domains.[3]

Registrar

SIP user agent registration to SIP registrar with authentication.
A registrar is a SIP endpoint that provides a location service. It accepts REGISTER requests, recording the address and other parameters from the user agent. For subsequent requests, it provides an essential means to locate possible communication peers on the network. The location service links one or more IP addresses to the SIP URI of the registering agent. Multiple user agents may register for the same URI, with the result that all registered user agents receive the calls to the URI.[3]

SIP registrars are logical elements and are often co-located with SIP proxies. To improve network scalability, location services may instead be located with a redirect server.[3]

Session border controller

Establishment of a session through a back-to-back user agent.
Session border controllers (SBCs) serve as middleboxes between user agents and SIP servers for various types of functions, including network topology hiding and assistance in NAT traversal. SBCs are an independently engineered solution and are not mentioned in the SIP RFC.[16]

Gateway
Gateways can be used to interconnect a SIP network to other networks, such as the PSTN, which use different protocols or technologies.[3]

SIP messages
SIP is a text-based protocol with syntax similar to that of HTTP. There are two different types of SIP messages: requests and responses. The first line of a request has a method, defining the nature of the request, and a Request-URI, indicating where the request should be sent.[23] The first line of a response has a response code.

Requests
Requests initiate a functionality of the protocol. They are sent by a user agent client to the server and are answered with one or more SIP responses, which return a result code of the transaction, and generally indicate the success, failure, or other state of the transaction.

SIP requests
Request name	Description	Notes	RFC references
REGISTER	Register the URI listed in the To-header field with a location server and associates it with the network address given in a Contact header field.	The command implements a location service.	RFC 3261
INVITE	Initiate a dialog for establishing a call. The request is sent by a user agent client to a user agent server.	When sent during an established dialog (reinvite) it modifies the sessions, for example placing a call on hold.	RFC 3261
ACK	Confirm that an entity has received a final response to an INVITE request.		RFC 3261
BYE	Signal termination of a dialog and end a call.	This message may be sent by either endpoint of a dialog.	RFC 3261
CANCEL	Cancel any pending request.	Usually means terminating a call while it is still ringing, before answer.	RFC 3261
UPDATE	Modify the state of a session without changing the state of the dialog.		RFC 3311
REFER	Ask recipient to issue a request for the purpose of call transfer.		RFC 3515
PRACK	Provisional acknowledgement.	PRACK is sent in response to provisional response (1xx).	RFC 3262
SUBSCRIBE	Initiates a subscription for notification of events from a notifier.		RFC 6665
NOTIFY	Inform a subscriber of notifications of a new event.		RFC 6665
PUBLISH	Publish an event to a notification server.		RFC 3903
MESSAGE	Deliver a text message.	Used in instant messaging applications.	RFC 3428
INFO	Send mid-session information that does not modify the session state.	This method is often used for DTMF relay.	RFC 6086
OPTIONS	Query the capabilities of an endpoint.	It is often used for NAT keepalive purposes.	RFC 3261
Responses
Main article: List of SIP response codes
Responses are sent by the user agent server indicating the result of a received request. Several classes of responses are recognized, determined by the numerical range of result codes:[24]

1xx: Provisional responses to requests indicate the request was valid and is being processed.
2xx: Successful completion of the request. As a response to an INVITE, it indicates a call is established. The most common code is 200, which is an unqualified success report.
3xx: Call redirection is needed for completion of the request. The request must be completed with a new destination.
4xx: The request cannot be completed at the server for a variety of reasons, including bad request syntax (code 400).
5xx: The server failed to fulfill an apparently valid request, including server internal errors (code 500).
6xx: The request cannot be fulfilled at any server. It indicates a global failure, including call rejection by the destination.
Transactions

If no response is received after a timer-controlled wait period the UAC may choose to terminate the transaction or retransmit the INVITE. Once a response is received, User1 is confident the INVITE was delivered reliably. User1's UAC must then acknowledge the response. On delivery of the ACK (2), both sides of the transaction are complete. In this case, a dialog may have been established.[25]

SIP defines a transaction mechanism to control the exchanges between participants and deliver messages reliably. A transaction is a state of a session, which is controlled by various timers. Client transactions send requests and server transactions respond to those requests with one or more responses. The responses may include provisional responses with a response code in the form 1xx, and one or multiple final responses (2xx – 6xx).[25]

Transactions are further categorized as either type invite or type non-invite. Invite transactions differ in that they can establish a long-running conversation, referred to as a dialog in SIP, and so include an acknowledgment (ACK) of any non-failing final response, e.g., 200 OK.[25]

Instant messaging and presence
The Session Initiation Protocol for Instant Messaging and Presence Leveraging Extensions (SIMPLE) is the SIP-based suite of standards for instant messaging and presence information. Message Session Relay Protocol (MSRP) allows instant message sessions and file transfer.[3]

Conformance testing
The SIP developer community meets regularly at conferences organized by SIP Forum to test interoperability of SIP implementations.[26] The TTCN-3 test specification language, developed by a task force at ETSI (STF 196), is used for specifying conformance tests for SIP implementations.[27]

Performance testing
When developing SIP software or deploying a new SIP infrastructure, it is important to test the capability of servers and IP networks to handle certain call load: number of concurrent calls and number of calls per second. SIP performance tester software is used to simulate SIP and RTP traffic to see if the server and IP network are stable under the call load.[28] The software measures performance indicators like answer delay, answer/seizure ratio, RTP jitter and packet loss, round-trip delay time.

Applications
SIP connection is a marketing term for voice over Internet Protocol (VoIP) services offered by many Internet telephony service providers (ITSPs). The service provides routing of telephone calls from a client's private branch exchange (PBX) telephone system to the PSTN. Such services may simplify corporate information system infrastructure by sharing Internet access for voice and data, and removing the cost for Basic Rate Interface (BRI) or Primary Rate Interface (PRI) telephone circuits.

SIP trunking is a similar marketing term preferred for when the service is used to simplify a telecom infrastructure by sharing the carrier access circuit for voice, data, and Internet traffic while removing the need for PRI circuits.[29][30]

SIP-enabled video surveillance cameras can initiate calls to alert the operator of events, such as the motion of objects in a protected area.

SIP is used in audio over IP for broadcasting applications where it provides an interoperable means for audio interfaces from different manufacturers to make connections with one another.[31]

Implementations
The U.S. National Institute of Standards and Technology (NIST), Advanced Networking Technologies Division provides a public-domain Java implementation[32] that serves as a reference implementation for the standard. The implementation can work in proxy server or user agent scenarios and has been used in numerous commercial and research projects. It supports RFC 3261 in full and a number of extension RFCs including RFC 6665 (event notification) and RFC 3262 (reliable provisional responses).

Numerous other commercial and open-source SIP implementations exist. See List of SIP software.

SIP-ISUP interworking
SIP-I, Session Initiation Protocol with encapsulated ISUP, is a protocol used to create, modify, and terminate communication sessions based on ISUP using SIP and IP networks. Services using SIP-I include voice, video telephony, fax and data. SIP-I and SIP-T[33] are two protocols with similar features, notably to allow ISUP messages to be transported over SIP networks. This preserves all of the detail available in the ISUP header.[a] SIP-I was defined by the ITU-T, whereas SIP-T was defined by the IETF.[34]

Encryption
Concerns about the security of calls via the public Internet have been addressed by encryption of the SIP protocol for secure transmission. The URI scheme SIPS is used to mandate that SIP communication be secured with Transport Layer Security (TLS).

End-to-end encryption of SIP is only possible if there is a direct connection between communication endpoints. While a direct connection can be made via Peer-to-peer SIP or via a VPN between the endpoints, most SIP communication involves multiple hops, with the first hop being from a user agent to the user agent's ITSP. For the multiple-hop case, SIPS will only secure the first hop; the remaining hops will normally not be secured with TLS and the SIP communication will be insecure. In contrast, the HTTPS protocol provides end-to-end security as it is done with a direct connection and does not involve the notion of hops.

The media streams (audio and video), which are separate connections from the SIPS signaling stream, may be encrypted using SRTP. The key exchange for SRTP is performed with SDES (RFC 4568), or with ZRTP (RFC 6189). When SDES is used, the keys will be transmitted via insecure SIP unless SIPS is used. One may also add a MIKEY (RFC 3830) exchange to SIP to determine session keys for use with SRTP.

---

Remote procedure call

In distributed computing, a remote procedure call (RPC) is an action in which a computer program causes a procedure (subroutine) to execute in a different address space of the current process (commonly on another computer on a shared computer network), which is written as if it were a normal (local) procedure call, without the programmer explicitly writing the details for the remote interaction. That is, the programmer writes essentially the same code whether the subroutine is local to the executing program, or remote. This is a form of server interaction (caller is client, executor is server), typically implemented via a request–response message passing system. In the object-oriented programming paradigm, RPCs are represented by remote method invocation (RMI). The RPC model implies a level of location transparency, namely that calling procedures are largely the same whether they are local or remote, but usually, they are not identical, so local calls can be distinguished from remote calls. Remote calls are usually orders of magnitude slower and less reliable than local calls, so distinguishing them is important.

RPCs are a form of inter-process communication (IPC), in that different processes have different address spaces: if on the same host machine, they have distinct virtual address spaces, even though the physical address space is the same; while if they are on different hosts, the physical address space is also different. Many different (often incompatible) technologies have been used to implement the concept. Modern RPC frameworks, such as gRPC and Apache Thrift, enhance the basic RPC model by using efficient binary serialization (e.g., Protocol Buffers), HTTP/2 multiplexing, and built-in support for features such as authentication, load balancing, streaming, and error handling, making them well-suited for building scalable microservices and enabling language interoperability.[1]

History and origins
Request–response protocols date to early distributed computing in the late 1960s, theoretical proposals of remote procedure calls as the model of network operations date to the 1970s, and practical implementations date to the early 1980s. Bruce Jay Nelson is generally credited with coining the term "remote procedure call" in 1981.[2]

Remote procedure calls used in modern operating systems trace their roots back to the RC 4000 multiprogramming system,[3] which used a request-response communication protocol for process synchronization.[4] The idea of treating network operations as remote procedure calls goes back at least to the 1970s in early ARPANET documents.[5] In 1978, Per Brinch Hansen proposed Distributed Processes, a language for distributed computing based on "external requests" consisting of procedure calls between processes.[6]

One of the earliest practical implementations was in 1982 by Brian Randell and colleagues for their Newcastle Connection between UNIX machines.[7] This was soon followed by "Lupine" by Andrew Birrell and Bruce Nelson in the Cedar environment at Xerox PARC.[8][9][10] Lupine automatically generated stubs, providing type-safe bindings, and used an efficient protocol for communication.[9] One of the first business uses of RPC was by Xerox under the name "Courier" in 1981. The first popular implementation of RPC on Unix was Sun's RPC (now called ONC RPC), used as the basis for Network File System (NFS).

In the 1990s, with the popularity of object-oriented programming, an alternative model of remote method invocation (RMI) was widely implemented, such as in Common Object Request Broker Architecture (CORBA, 1991) and Java remote method invocation. RMIs, in turn, fell in popularity with the rise of the internet, particularly in the 2000s.

Message passing
RPC is a request–response protocol. An RPC is initiated by the client, which sends a request message to a known remote server to execute a specified procedure with supplied parameters. The remote server sends a response to the client, and the application continues its process. While the server is processing the call, the client is blocked (it waits until the server has finished processing before resuming execution), unless the client sends an asynchronous request to the server, such as an XMLHttpRequest. There are many variations and subtleties in various implementations, resulting in a variety of different (incompatible) RPC protocols.

An important difference between remote procedure calls and local calls is that remote calls can fail because of unpredictable network problems. Also, callers generally must deal with such failures without knowing whether the remote procedure was actually invoked. Idempotent procedures (those that have no additional effects if called more than once) are easily handled, but enough difficulties remain that code to call remote procedures is often confined to carefully written low-level subsystems.

Sequence of events
The client calls the client stub. The call is a local procedure call, with parameters pushed on to the stack in the normal way.
The client stub packs the parameters into a message and makes a system call to send the message. Packing the parameters is called marshalling.
The client's local operating system sends the message from the client machine to the server machine.
The local operating system on the server machine passes the incoming packets to the server stub.
The server stub unpacks the parameters from the message. Unpacking the parameters is called unmarshalling.
Finally, the server stub calls the server procedure. The reply traces the same steps in the reverse direction.
Standard contact mechanisms
To let different clients access servers, a number of standardized RPC systems have been created. Most of these use an interface description language (IDL) to let various platforms call the RPC. The IDL files can then be used to generate code to interface between the client and servers.

Analogues
Notable RPC implementations and analogues include:

Language-specific
Java's Remote Method Invocation (Java RMI) API provides similar functionality to standard Unix RPC methods.
Go provides a package rpc for implementing RPC, with support for asynchronous calls.
Modula-3's network objects, which were the basis for Java's RMI[11]
RPyC implements RPC mechanisms in Python, with support for asynchronous calls.
Distributed Ruby (DRb) allows Ruby programs to communicate with each other on the same machine or over a network. DRb uses remote method invocation (RMI) to pass commands and data between processes.
Erlang is process-oriented and natively supports distribution and RPCs via message passing between nodes and local processes alike.
Elixir builds on top of the Erlang. It allows process communication (Elixir/Erlang processes, not OS processes) of the same network out of the box via Agents and message passing.
Google's Rust RPC framework Tarpc lets developers define the structure of messages using Rust's structs and traits, rather than using protobuf.[12]
Application-specific
Action Message Format (AMF) allows Adobe Flex applications to communicate with back-ends or other applications that support AMF.
Remote Function Call is the standard SAP interface for communication between SAP systems. RFC calls a function to be executed in a remote system.
General
NFS (Network File System) is one of the most prominent users of RPC
Open Network Computing RPC, by Sun Microsystems (also known as Sun RPC)
D-Bus open source IPC program provides similar function to CORBA.
SORCER provides the API and exertion-oriented language (EOL) for a federated method invocation
XML-RPC is an RPC protocol that uses XML to encode its calls and HTTP as a transport mechanism.
JSON-RPC is an RPC protocol that uses JSON-encoded messages.
JSON-WSP is an RPC protocol that is inspired from JSON-RPC.
SOAP is a successor of XML-RPC and also uses XML to encode its HTTP-based calls.
ZeroC's Internet Communications Engine (Ice) distributed computing platform.
Etch framework for building network services.
Apache Thrift protocol and framework.
CORBA provides remote procedure invocation through an intermediate layer called the object request broker.
Libevent provides a framework for creating RPC servers and clients.[13]
Windows Communication Foundation is an application programming interface in the .NET framework for building connected, service-oriented applications.
Microsoft .NET Remoting offers RPC facilities for distributed systems implemented on the Windows platform. It has been superseded by WCF.
The Microsoft DCOM uses MSRPC which is based on DCE/RPC
The Open Software Foundation DCE/RPC Distributed Computing Environment (also implemented by Microsoft).
Google Protocol Buffers (protobufs) package includes an interface definition language used for its RPC protocols[14] open sourced in 2015 as gRPC.[1]
WAMP combines RPC and Publish-Subscribe into a single, transport-agnostic protocol.
Google Web Toolkit uses an asynchronous RPC to communicate to the server service.[15]
Apache Avro provides RPC where client and server exchange schemas in the connection handshake and code generation is not required.
See also
9P
Microsoft RPC
Local Inter-Process Communication
HTTP
ODBC
Remote evaluation
External Data Representation (serialization format used by e.g. NFS)
Network Data Representation (serialization format used by e.g. Microsoft RPC)
Resource-oriented architecture
Distributed object middleware
Fragmented object
gRPC