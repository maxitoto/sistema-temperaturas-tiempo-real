#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP_SSLClient.h>
#include <mbedtls/base64.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

namespace SmtpNet {

    struct ConfigSmtpEnvio {
        const char* host            = "";
        uint16_t    puerto          = 465;
        bool        usarTls         = true;
        const char* usuario         = "";
        const char* password        = "";
        const char* emailRemitente  = "";
        const char* nombreRemitente = "Laboratorio UNTDF";
    };

    struct ResultadoSmtp {
        uint16_t codigoSmtp             = 0;
        bool     exito                  = false;
        uint8_t  destinatariosAceptados = 0;
        char     detalle[128]           = "";
    };

    class ClienteSmtpTls {
    private:
        static void copiarTexto(char* dst, size_t cap, const char* src) {
            if (!dst || cap == 0) return;
            if (!src) { dst[0] = '\0'; return; }
            strncpy(dst, src, cap - 1);
            dst[cap - 1] = '\0';
        }

        // Codifica a Base64 (si quitarEspacios = true, limpia los espacios de las App Passwords de Google)
        static bool codificarBase64(const char* src, bool quitarEspacios, char* outBuf, size_t outCap) {
            if (!src || !outBuf || outCap == 0) return false;
            char limpio[160]{};
            size_t lenLimpio = 0;
            for (size_t i = 0; src[i] != '\0' && lenLimpio + 1 < sizeof(limpio); ++i) {
                if (quitarEspacios && src[i] == ' ') continue;
                limpio[lenLimpio++] = src[i];
            }
            limpio[lenLimpio] = '\0';

            size_t outLen = 0;
            const int rc = mbedtls_base64_encode(
                reinterpret_cast<unsigned char*>(outBuf),
                outCap - 1,
                &outLen,
                reinterpret_cast<const unsigned char*>(limpio),
                lenLimpio
            );
            if (rc != 0) return false;
            outBuf[outLen] = '\0';
            return true;
        }

        // Lee respuesta SMTP completa (incluyendo respuestas multilínea "250-..." hasta "250 ...")
        static bool leerRespuesta(
            Client& cli,
            uint16_t& codigoOut,
            char* ultimaLineaOut,
            size_t capLinea,
            uint32_t timeoutMs = 5000
        ) {
            codigoOut = 0;
            if (ultimaLineaOut && capLinea > 0) ultimaLineaOut[0] = '\0';

            const uint32_t inicioMs = millis();
            char linea[192]{};

            while ((millis() - inicioMs) < timeoutMs) {
                size_t len = 0;
                bool llegoFinLinea = false;

                while ((millis() - inicioMs) < timeoutMs) {
                    if (cli.available() > 0) {
                        const int ch = cli.read();
                        if (ch < 0) continue;
                        if (ch == '\r') continue;
                        if (ch == '\n') {
                            llegoFinLinea = true;
                            break;
                        }
                        if (len + 1 < sizeof(linea)) {
                            linea[len++] = static_cast<char>(ch);
                        }
                    } else {
                        if (!cli.connected()) break;
                        vTaskDelay(1);
                    }
                }
                linea[len] = '\0';

                if (!llegoFinLinea && len == 0) {
                    return false;
                }

                if (ultimaLineaOut && capLinea > 0 && len > 0) {
                    copiarTexto(ultimaLineaOut, capLinea, linea);
                }

                if (len >= 3 &&
                    isdigit(static_cast<unsigned char>(linea[0])) &&
                    isdigit(static_cast<unsigned char>(linea[1])) &&
                    isdigit(static_cast<unsigned char>(linea[2]))) {
                    codigoOut = static_cast<uint16_t>(
                        (linea[0] - '0') * 100 +
                        (linea[1] - '0') * 10 +
                        (linea[2] - '0')
                    );
                    // Guion en posición 3 indica que siguen más líneas de la misma respuesta
                    if (len >= 4 && linea[3] == '-') {
                        continue;
                    }
                    return true;
                }
            }
            return false;
        }

        static bool enviarComandoYValidar(
            Client& cli,
            const char* comando,
            uint16_t codigoEsperadoMin,
            uint16_t codigoEsperadoMax,
            uint16_t& codigoOut,
            char* respOut,
            size_t capResp
        ) {
            if (comando && comando[0] != '\0') {
                cli.print(comando);
                cli.print("\r\n");
            }
            if (!leerRespuesta(cli, codigoOut, respOut, capResp)) {
                copiarTexto(respOut, capResp, "Timeout esperando respuesta del servidor SMTP");
                return false;
            }
            return (codigoOut >= codigoEsperadoMin && codigoOut <= codigoEsperadoMax);
        }

        static ResultadoSmtp ejecutarSesionSmtp(
            Client& cli,
            const ConfigSmtpEnvio& cfg,
            const char* const* destinatarios,
            size_t cantDestinatarios,
            const char* asunto,
            const char* cuerpo,
            bool* aceptadoPorIndice,
            bool esHtml = false
        ) {
            ResultadoSmtp res{};
            char resp[128]{};
            char cmd[224]{};

            // 1. Banner inicial (220)
            if (!leerRespuesta(cli, res.codigoSmtp, resp, sizeof(resp)) || res.codigoSmtp != 220) {
                snprintf(res.detalle, sizeof(res.detalle), "Banner SMTP invalido (%u): %.80s",
                         static_cast<unsigned>(res.codigoSmtp), resp);
                return res;
            }

            // 2. EHLO (250)
            if (!enviarComandoYValidar(cli, "EHLO esp32-untdf.local", 250, 250, res.codigoSmtp, resp, sizeof(resp))) {
                snprintf(res.detalle, sizeof(res.detalle), "EHLO rechazado (%u): %.80s",
                         static_cast<unsigned>(res.codigoSmtp), resp);
                return res;
            }

            // 3. Autenticación AUTH LOGIN (si hay usuario/clave)
            const char* userAuth = (cfg.usuario && cfg.usuario[0] != '\0') ? cfg.usuario : cfg.emailRemitente;
            const char* fromAddr = (cfg.emailRemitente && cfg.emailRemitente[0] != '\0') ? cfg.emailRemitente : userAuth;

            if (userAuth && userAuth[0] != '\0' && cfg.password && cfg.password[0] != '\0') {
                if (!enviarComandoYValidar(cli, "AUTH LOGIN", 334, 334, res.codigoSmtp, resp, sizeof(resp))) {
                    snprintf(res.detalle, sizeof(res.detalle), "AUTH LOGIN fallo (%u): %.80s",
                             static_cast<unsigned>(res.codigoSmtp), resp);
                    return res;
                }

                char b64[224]{};
                if (!codificarBase64(userAuth, false, b64, sizeof(b64)) ||
                    !enviarComandoYValidar(cli, b64, 334, 334, res.codigoSmtp, resp, sizeof(resp))) {
                    snprintf(res.detalle, sizeof(res.detalle), "Usuario SMTP rechazado (%u): %.70s",
                             static_cast<unsigned>(res.codigoSmtp), resp);
                    return res;
                }

                // Quita espacios de la App Password de Google (ej. "abcd efgh ijkl mnop")
                if (!codificarBase64(cfg.password, true, b64, sizeof(b64)) ||
                    !enviarComandoYValidar(cli, b64, 235, 235, res.codigoSmtp, resp, sizeof(resp))) {
                    snprintf(res.detalle, sizeof(res.detalle), "Auth SMTP fallida (%u): verifique App Password",
                             static_cast<unsigned>(res.codigoSmtp));
                    return res;
                }
            }

            // 4. MAIL FROM
            snprintf(cmd, sizeof(cmd), "MAIL FROM:<%s>", fromAddr ? fromAddr : "");
            if (!enviarComandoYValidar(cli, cmd, 250, 250, res.codigoSmtp, resp, sizeof(resp))) {
                snprintf(res.detalle, sizeof(res.detalle), "MAIL FROM rechazado (%u): %.70s",
                         static_cast<unsigned>(res.codigoSmtp), resp);
                return res;
            }

            // 5. RCPT TO para todos los destinatarios en la misma sesión TLS
            uint8_t aceptados = 0;
            for (size_t i = 0; i < cantDestinatarios; ++i) {
                if (aceptadoPorIndice) aceptadoPorIndice[i] = false;
                if (!destinatarios[i] || destinatarios[i][0] == '\0') continue;

                snprintf(cmd, sizeof(cmd), "RCPT TO:<%s>", destinatarios[i]);
                uint16_t rcptCode = 0;
                if (enviarComandoYValidar(cli, cmd, 250, 251, rcptCode, resp, sizeof(resp))) {
                    aceptados++;
                    if (aceptadoPorIndice) aceptadoPorIndice[i] = true;
                }
                res.codigoSmtp = rcptCode;
            }

            if (aceptados == 0) {
                snprintf(res.detalle, sizeof(res.detalle), "Ningun destinatario aceptado (%u): %.64s",
                         static_cast<unsigned>(res.codigoSmtp), resp);
                return res;
            }

            // 6. DATA (354)
            if (!enviarComandoYValidar(cli, "DATA", 354, 354, res.codigoSmtp, resp, sizeof(resp))) {
                snprintf(res.detalle, sizeof(res.detalle), "DATA rechazado (%u): %.80s",
                         static_cast<unsigned>(res.codigoSmtp), resp);
                return res;
            }

            // 7. Cabeceras RFC 5322 + Cuerpo en texto UTF-8
            const char* nomRem = (cfg.nombreRemitente && cfg.nombreRemitente[0] != '\0')
                ? cfg.nombreRemitente
                : "Laboratorio UNTDF";

            cli.print("From: \"");
            cli.print(nomRem);
            cli.print("\" <");
            cli.print(fromAddr ? fromAddr : "");
            cli.print(">\r\nTo: ");
            bool primeroTo = true;
            for (size_t i = 0; i < cantDestinatarios; ++i) {
                if (aceptadoPorIndice && !aceptadoPorIndice[i]) continue;
                if (!destinatarios[i] || destinatarios[i][0] == '\0') continue;
                if (!primeroTo) cli.print(", ");
                cli.print(destinatarios[i]);
                primeroTo = false;
            }
            cli.print("\r\nSubject: ");
            {
                const char* subjRaw = (asunto && asunto[0] != '\0') ? asunto : "[UNTDF ESP32] Notificacion";
                char subjB64[180]{};
                if (codificarBase64(subjRaw, false, subjB64, sizeof(subjB64))) {
                    cli.print("=?UTF-8?B?");
                    cli.print(subjB64);
                    cli.print("?=");
                } else {
                    cli.print(subjRaw);
                }
            }
            cli.print(esHtml
                ? "\r\nMIME-Version: 1.0\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
                : "\r\nMIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\n");

            // Envío por líneas con dot-stuffing RFC 5321 (mucho más rápido sobre TLS que byte a byte)
            if (cuerpo) {
                const char* p = cuerpo;
                while (*p) {
                    const char* fin = p;
                    while (*fin && *fin != '\n' && *fin != '\r') {
                        ++fin;
                    }
                    if (*p == '.') {
                        cli.write('.');
                    }
                    const size_t lenLinea = static_cast<size_t>(fin - p);
                    if (lenLinea > 0) {
                        cli.write(reinterpret_cast<const uint8_t*>(p), lenLinea);
                    }
                    if (*fin == '\r' && *(fin + 1) == '\n') {
                        cli.print("\r\n");
                        p = fin + 2;
                    } else if (*fin == '\n' || *fin == '\r') {
                        cli.print("\r\n");
                        p = fin + 1;
                    } else {
                        p = fin;
                    }
                }
            }

            // Cierre del mensaje DATA (<CRLF>.<CRLF>) -> esperar 250
            if (!enviarComandoYValidar(cli, "\r\n.", 250, 250, res.codigoSmtp, resp, sizeof(resp))) {
                snprintf(res.detalle, sizeof(res.detalle), "Envio DATA rechazado (%u): %.75s",
                         static_cast<unsigned>(res.codigoSmtp), resp);
                return res;
            }

            // 8. QUIT limpio
            uint16_t quitCode = 0;
            enviarComandoYValidar(cli, "QUIT", 221, 221, quitCode, resp, sizeof(resp));

            res.exito                  = true;
            res.destinatariosAceptados = aceptados;
            snprintf(res.detalle, sizeof(res.detalle), "SMTP 250 OK · enviado a %u destinatario(s)",
                     static_cast<unsigned>(aceptados));
            return res;
        }

    public:
        __attribute__((noinline))
        static ResultadoSmtp enviar(
            const ConfigSmtpEnvio& cfg,
            const char* const* destinatarios,
            size_t cantDestinatarios,
            const char* asunto,
            const char* cuerpo,
            bool* aceptadoPorIndice = nullptr,
            bool esHtml = false
        ) {
            ResultadoSmtp res{};

            if (WiFi.status() != WL_CONNECTED) {
                copiarTexto(res.detalle, sizeof(res.detalle), "WiFi STA sin conexion a internet");
                return res;
            }
            if (!cfg.host || cfg.host[0] == '\0' || cfg.puerto == 0) {
                copiarTexto(res.detalle, sizeof(res.detalle), "Host o puerto SMTP no configurado");
                return res;
            }
            if (!destinatarios || cantDestinatarios == 0) {
                copiarTexto(res.detalle, sizeof(res.detalle), "Sin destinatarios para enviar");
                return res;
            }
            if (cfg.usarTls && cfg.puerto == 587) {
                copiarTexto(res.detalle, sizeof(res.detalle),
                            "Use puerto 465 (SMTPS TLS implicito) para Gmail en ESP32");
                return res;
            }

            // 1. Verificar DNS antes de abrir TLS para separar fallos de red/router de fallos SSL
            IPAddress ipDestino;
            if (!WiFi.hostByName(cfg.host, ipDestino)) {
                snprintf(res.detalle, sizeof(res.detalle),
                         "DNS fallo para %.32s (GW:%s DNS:%s)",
                         cfg.host,
                         WiFi.gatewayIP().toString().c_str(),
                         WiFi.dnsIP().toString().c_str());
                return res;
            }

            if (cfg.usarTls) {
                const uint32_t heapLibre = ESP.getFreeHeap();
                const uint32_t maxBloque = ESP.getMaxAllocHeap();
                if (heapLibre < 20000 || maxBloque < 10000) {
                    snprintf(res.detalle, sizeof(res.detalle),
                             "Heap insuficiente para BearSSL (libre:%uB maxBlk:%uB)",
                             static_cast<unsigned>(heapLibre),
                             static_cast<unsigned>(maxBloque));
                    return res;
                }

                WiFiClient tcpBase;
                tcpBase.setTimeout(6000);

                ESP_SSLClient ssl;
                ssl.setClient(&tcpBase);
                ssl.setInsecure();
                // Buffers compactos: ~3 KB totales en vez de ~20+ KB fijos de mbedTLS
                ssl.setBufferSizes(2048, 1024);
                // Nivel 0 obligatorio para no escribir texto en UART0 (compartido con UART-S)
                ssl.setDebugLevel(0);

                if (!ssl.connect(cfg.host, cfg.puerto)) {
                    snprintf(res.detalle, sizeof(res.detalle),
                             "BearSSL fallo al conectar a %s (%s):%u | H:%u/%u",
                             cfg.host,
                             ipDestino.toString().c_str(),
                             static_cast<unsigned>(cfg.puerto),
                             static_cast<unsigned>(ESP.getFreeHeap()),
                             static_cast<unsigned>(ESP.getMaxAllocHeap()));
                    ssl.stop();
                    tcpBase.stop();
                    return res;
                }

                res = ejecutarSesionSmtp(ssl, cfg, destinatarios, cantDestinatarios, asunto, cuerpo, aceptadoPorIndice, esHtml);
                ssl.stop();
                tcpBase.stop();
                return res;
            }

            WiFiClient plain;
            plain.setTimeout(5);
            if (plain.getTimeout() < 1000) {
                plain.setTimeout(5000);
            }
            if (!plain.connect(cfg.host, cfg.puerto)) {
                snprintf(res.detalle, sizeof(res.detalle),
                         "No se pudo conectar por TCP a %s (%s):%u",
                         cfg.host, ipDestino.toString().c_str(), static_cast<unsigned>(cfg.puerto));
                return res;
            }

            res = ejecutarSesionSmtp(plain, cfg, destinatarios, cantDestinatarios, asunto, cuerpo, aceptadoPorIndice, esHtml);
            plain.stop();
            return res;
        }
    };

} // namespace SmtpNet