#ifndef WIFI_SECURITY_ENCRYPT_HPP
#define WIFI_SECURITY_ENCRYPT_HPP

#include <Arduino.h>
#include <mbedtls/gcm.h>
#include <esp_random.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Activar/desactivar cifrado en tiempo de compilación (-DWIFI_CIFRADO_CCM_ACTIVO=0 para pruebas sin cifrado)
#ifndef WIFI_CIFRADO_CCM_ACTIVO
#define WIFI_CIFRADO_CCM_ACTIVO 1
#endif

namespace WiFiSecurity {

    enum CanalDireccionCcm : uint8_t {
        DIR_C2S_TCP = 0x01,
        DIR_S2C_TCP = 0x02,
        DIR_S2C_UDP = 0x03
    };

    enum class ResultadoDescifradoCcm : uint8_t {
        OK = 0,
        ERROR_FORMATO,
        ERROR_VERSION_O_CANAL,
        ERROR_REPLAY,
        ERROR_TAG_AUTENTICACION,
        ERROR_MOTOR_CRYPTO
    };

    struct EstadoCanalCcmSlot {
        uint32_t rxSalt{0};
        uint32_t ultimoRxSeq{0};
        bool     rxInicializado{false};

        uint32_t txSalt{0};
        uint32_t txSeq{0};

        void reiniciar() {
            rxSalt         = 0;
            ultimoRxSeq    = 0;
            rxInicializado = false;
            txSalt         = esp_random();
            if (txSalt == 0) txSalt = 0xA5A55A5Au;
            txSeq          = 0;
        }
    };

    // AEAD AES-128-GCM (compatible nativamente con Electron/BoringSSL y acelerado por HW en ESP32)
    class CifradorAesCcm {
    public:
        static constexpr uint8_t VERSION_PROTOCOLO = 0x01;
        static constexpr size_t  TAMANO_CLAVE      = 16; // AES-128
        static constexpr size_t  TAMANO_NONCE      = 12; // 96 bits estándar GCM
        static constexpr size_t  TAMANO_CABECERA   = 10; // [VER:1][FLAGS:1][SALT:4][SEQ:4]
        static constexpr size_t  TAMANO_TAG        = 16; // Tag GCM de 128 bits
        static constexpr size_t  OVERHEAD_INTERNO  = TAMANO_CABECERA + TAMANO_TAG; // 26 bytes

    private:
        mbedtls_gcm_context ctx{};
        bool inicializado{false};

        static inline void escribirU32BE(uint8_t* dst, uint32_t val) {
            dst[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
            dst[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
            dst[2] = static_cast<uint8_t>((val >> 8)  & 0xFF);
            dst[3] = static_cast<uint8_t>(val & 0xFF);
        }

        static inline uint32_t leerU32BE(const uint8_t* src) {
            return (static_cast<uint32_t>(src[0]) << 24) |
                   (static_cast<uint32_t>(src[1]) << 16) |
                   (static_cast<uint32_t>(src[2]) << 8)  |
                   static_cast<uint32_t>(src[3]);
        }

        static inline void construirNonce(
            uint8_t version,
            uint8_t flags,
            uint32_t salt,
            uint32_t seq,
            uint8_t nonceOut[TAMANO_NONCE]
        ) {
            nonceOut[0] = version;
            nonceOut[1] = flags;
            nonceOut[2] = 0x00;
            nonceOut[3] = 0x00;
            escribirU32BE(nonceOut + 4, salt);
            escribirU32BE(nonceOut + 8, seq);
        }

    public:
        CifradorAesCcm() {
            mbedtls_gcm_init(&ctx);
        }

        ~CifradorAesCcm() {
            mbedtls_gcm_free(&ctx);
        }

        CifradorAesCcm(const CifradorAesCcm&) = delete;
        CifradorAesCcm& operator=(const CifradorAesCcm&) = delete;

        bool inicializarConPsk(const uint8_t clavePsk16[TAMANO_CLAVE]) {
            if (clavePsk16 == nullptr) return false;
            const int rc = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, clavePsk16, 128);
            inicializado = (rc == 0);
            return inicializado;
        }

        bool estaInicializado() const {
            return inicializado;
        }

        size_t empaquetarYCifrarFrame(
            CanalDireccionCcm direccion,
            EstadoCanalCcmSlot& estado,
            const uint8_t* textoPlanoSinLen,
            size_t lenPlano,
            uint8_t* frameSalidaConLen,
            size_t capSalida
        ) {
            if (!inicializado || textoPlanoSinLen == nullptr || frameSalidaConLen == nullptr || lenPlano == 0) {
                return 0;
            }
            if (estado.txSeq >= 0xFFFFFFFEu) {
                return 0;
            }

            const size_t lenInterno = OVERHEAD_INTERNO + lenPlano;
            const size_t lenTotal   = 4 + lenInterno;
            if (capSalida < lenTotal) {
                return 0;
            }

            const uint32_t seqActual = ++estado.txSeq;
            if (estado.txSalt == 0) {
                estado.txSalt = esp_random();
                if (estado.txSalt == 0) estado.txSalt = 0xA5A55A5Au;
            }

            escribirU32BE(frameSalidaConLen, static_cast<uint32_t>(lenInterno));

            uint8_t* hdr = frameSalidaConLen + 4;
            hdr[0] = VERSION_PROTOCOLO;
            hdr[1] = static_cast<uint8_t>(direccion);
            escribirU32BE(hdr + 2, estado.txSalt);
            escribirU32BE(hdr + 6, seqActual);

            uint8_t nonce[TAMANO_NONCE];
            construirNonce(VERSION_PROTOCOLO, static_cast<uint8_t>(direccion), estado.txSalt, seqActual, nonce);

            uint8_t* ptrCipher = hdr + TAMANO_CABECERA;
            uint8_t* ptrTag    = ptrCipher + lenPlano;

            const int rc = mbedtls_gcm_crypt_and_tag(
                &ctx,
                MBEDTLS_GCM_ENCRYPT,
                lenPlano,
                nonce, TAMANO_NONCE,
                hdr, TAMANO_CABECERA,
                textoPlanoSinLen,
                ptrCipher,
                TAMANO_TAG, ptrTag
            );

            return (rc == 0) ? lenTotal : 0;
        }

        ResultadoDescifradoCcm descifrarYValidar(
            CanalDireccionCcm direccionEsperada,
            EstadoCanalCcmSlot& estado,
            const uint8_t* frameInterno,
            size_t lenInterno,
            uint8_t* textoPlanoOut,
            size_t capPlanoOut,
            size_t& lenPlanoOut
        ) {
            lenPlanoOut = 0;
            if (!inicializado || frameInterno == nullptr || textoPlanoOut == nullptr) {
                return ResultadoDescifradoCcm::ERROR_MOTOR_CRYPTO;
            }
            if (lenInterno <= OVERHEAD_INTERNO) {
                return ResultadoDescifradoCcm::ERROR_FORMATO;
            }

            const size_t lenCipher = lenInterno - OVERHEAD_INTERNO;
            if (lenCipher > capPlanoOut) {
                return ResultadoDescifradoCcm::ERROR_FORMATO;
            }

            const uint8_t  ver   = frameInterno[0];
            const uint8_t  flags = frameInterno[1];
            const uint32_t salt  = leerU32BE(frameInterno + 2);
            const uint32_t seq   = leerU32BE(frameInterno + 6);

            if (ver != VERSION_PROTOCOLO || flags != static_cast<uint8_t>(direccionEsperada) || salt == 0 || seq == 0) {
                return ResultadoDescifradoCcm::ERROR_VERSION_O_CANAL;
            }

            if (estado.rxInicializado) {
                if (salt != estado.rxSalt || seq <= estado.ultimoRxSeq) {
                    return ResultadoDescifradoCcm::ERROR_REPLAY;
                }
            }

            uint8_t nonce[TAMANO_NONCE];
            construirNonce(ver, flags, salt, seq, nonce);

            const uint8_t* ptrCipher = frameInterno + TAMANO_CABECERA;
            const uint8_t* ptrTag    = ptrCipher + lenCipher;

            const int rc = mbedtls_gcm_auth_decrypt(
                &ctx,
                lenCipher,
                nonce, TAMANO_NONCE,
                frameInterno, TAMANO_CABECERA,
                ptrTag, TAMANO_TAG,
                ptrCipher,
                textoPlanoOut
            );

            if (rc != 0) {
                return ResultadoDescifradoCcm::ERROR_TAG_AUTENTICACION;
            }

            if (!estado.rxInicializado) {
                estado.rxSalt         = salt;
                estado.rxInicializado = true;
            }
            estado.ultimoRxSeq = seq;
            lenPlanoOut        = lenCipher;
            return ResultadoDescifradoCcm::OK;
        }
    };

    // PSK por defecto de desarrollo (16 bytes). Sustituir por carga desde NVS/config segura en producción.
    inline constexpr uint8_t PSK_DEFECTO_128[CifradorAesCcm::TAMANO_CLAVE] = {
        0x55, 0x4E, 0x54, 0x44, 0x46, 0x2D, 0x45, 0x53,
        0x50, 0x33, 0x32, 0x2D, 0x50, 0x53, 0x4B, 0x31
    };

} // namespace WiFiSecurity

#endif