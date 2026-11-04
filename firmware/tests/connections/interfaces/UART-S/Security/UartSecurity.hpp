#pragma once
#include <mbedtls/gcm.h>
#include <mbedtls/ecp.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/bignum.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/sha256.h>
#include <vector>
#include <string>
#include <stdint.h>
#include <string.h>

namespace Connections::UartS {

class UartSecurity {
public:
    UartSecurity() {
        mbedtls_gcm_init(&gcmTxCtx_);
        mbedtls_gcm_init(&gcmRxCtx_);
        mbedtls_ecp_group_init(&grp_);
        mbedtls_mpi_init(&d_);
        mbedtls_mpi_init(&z_);
        mbedtls_ecp_point_init(&Q_);
        mbedtls_ecp_point_init(&Qp_);
        mbedtls_entropy_init(&entropy_);
        mbedtls_ctr_drbg_init(&ctrDrbg_);
    }

    ~UartSecurity() {
        mbedtls_gcm_free(&gcmTxCtx_);
        mbedtls_gcm_free(&gcmRxCtx_);
        mbedtls_ecp_group_free(&grp_);
        mbedtls_mpi_free(&d_);
        mbedtls_mpi_free(&z_);
        mbedtls_ecp_point_free(&Q_);
        mbedtls_ecp_point_free(&Qp_);
        mbedtls_entropy_free(&entropy_);
        mbedtls_ctr_drbg_free(&ctrDrbg_);
    }

    void loadConfig() {
        // cargar de base de datos
        const char* pers = "esp32_asym_ecdh";
        mbedtls_ctr_drbg_seed(&ctrDrbg_, mbedtls_entropy_func, &entropy_,
                              reinterpret_cast<const unsigned char*>(pers), strlen(pers));
    }

    bool generateHandshakePublic(std::vector<uint8_t>& outPubKey) {
        mbedtls_ecp_group_free(&grp_);
        mbedtls_mpi_free(&d_);
        mbedtls_ecp_point_free(&Q_);
        mbedtls_ecp_group_init(&grp_);
        mbedtls_mpi_init(&d_);
        mbedtls_ecp_point_init(&Q_);

        if (mbedtls_ecp_group_load(&grp_, MBEDTLS_ECP_DP_SECP256R1) != 0) return false;
        if (mbedtls_ecdh_gen_public(&grp_, &d_, &Q_,
                                    mbedtls_ctr_drbg_random, &ctrDrbg_) != 0) return false;

        uint8_t buf[65];
        size_t olen = 0;
        if (mbedtls_ecp_point_write_binary(&grp_, &Q_, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                           &olen, buf, sizeof(buf)) != 0) return false;
        outPubKey.assign(buf, buf + olen);
        return true;
    }

    // Deriva claves separadas por dirección (c2s vs s2c) y por canal ("uart", "tcp", "udp") evitando colisión de nonces GCM
    bool computeSharedKey(const uint8_t* peerPub, size_t peerLen, bool isInitiator = false, const char* channelLabel = "uart") {
        if (peerLen < 65) return false;
        mbedtls_ecp_point_free(&Qp_);
        mbedtls_mpi_free(&z_);
        mbedtls_ecp_point_init(&Qp_);
        mbedtls_mpi_init(&z_);

        if (mbedtls_ecp_point_read_binary(&grp_, &Qp_, peerPub, 65) != 0) return false;
        if (mbedtls_ecdh_compute_shared(&grp_, &z_, &Qp_, &d_,
                                        mbedtls_ctr_drbg_random, &ctrDrbg_) != 0) return false;

        if (mbedtls_mpi_write_binary(&z_, rawSharedSecret_, sizeof(rawSharedSecret_)) != 0) return false;
        return setupDirectionalKeys(rawSharedSecret_, isInitiator, channelLabel);
    }

    bool deriveChannelKeysFrom(const UartSecurity& base, bool isInitiator, const char* channelLabel) {
        memcpy(rawSharedSecret_, base.rawSharedSecret_, sizeof(rawSharedSecret_));
        return setupDirectionalKeys(rawSharedSecret_, isInitiator, channelLabel);
    }

    // Formato: [Counter(8B LE)] + [Ciphertext(NB)] + [GCM_Tag(16B)]
    bool encrypt(const uint8_t* plain, size_t len, std::vector<uint8_t>& outCipher) {
        uint64_t counter = ++txCounter_;
        uint8_t iv[12] = {0};
        iv[0] = txDirByte_;
        iv[1] = channelByte_;
        memcpy(iv + 4, &counter, sizeof(counter));

        outCipher.resize(8 + len + 16);
        memcpy(outCipher.data(), &counter, 8);

        uint8_t* cipherPtr = outCipher.data() + 8;
        uint8_t* tagPtr = outCipher.data() + 8 + len;

        int ret = mbedtls_gcm_crypt_and_tag(&gcmTxCtx_, MBEDTLS_GCM_ENCRYPT, len,
                                            iv, sizeof(iv), outCipher.data(), 8,
                                            plain, cipherPtr, 16, tagPtr);
        return ret == 0;
    }

    bool decrypt(const uint8_t* inData, size_t len, std::vector<uint8_t>& outPlain) {
        if (len < 24) return false;
        uint64_t packetCounter = 0;
        memcpy(&packetCounter, inData, 8);

        if (packetCounter <= rxCounter_) return false;

        uint8_t iv[12] = {0};
        iv[0] = rxDirByte_;
        iv[1] = channelByte_;
        memcpy(iv + 4, &packetCounter, sizeof(packetCounter));

        size_t cipherLen = len - 24;
        const uint8_t* cipherPtr = inData + 8;
        const uint8_t* tagPtr = inData + 8 + cipherLen;

        outPlain.resize(cipherLen);
        int ret = mbedtls_gcm_auth_decrypt(&gcmRxCtx_, cipherLen, iv, sizeof(iv),
                                           inData, 8, tagPtr, 16, cipherPtr, outPlain.data());
        if (ret == 0) {
            rxCounter_ = packetCounter;
            return true;
        }
        return false;
    }

private:
    mbedtls_gcm_context gcmTxCtx_;
    mbedtls_gcm_context gcmRxCtx_;
    mbedtls_ecp_group grp_;
    mbedtls_mpi d_;
    mbedtls_mpi z_;
    mbedtls_ecp_point Q_;
    mbedtls_ecp_point Qp_;
    mbedtls_entropy_context entropy_;
    mbedtls_ctr_drbg_context ctrDrbg_;
    uint8_t rawSharedSecret_[32] = {0};
    uint8_t txKey_[32] = {0};
    uint8_t rxKey_[32] = {0};
    uint8_t txDirByte_ = 0x02;
    uint8_t rxDirByte_ = 0x01;
    uint8_t channelByte_ = 0x01;
    uint64_t txCounter_ = 0;
    uint64_t rxCounter_ = 0;

    static void deriveKeySha256(const uint8_t secret[32], const char* prefix, const char* label, uint8_t outKey[32]) {
        std::string info = std::string(prefix) + (label ? label : "uart");
        std::vector<uint8_t> buf(32 + info.size());
        memcpy(buf.data(), secret, 32);
        memcpy(buf.data() + 32, info.data(), info.size());
        mbedtls_sha256(buf.data(), buf.size(), outKey, 0);
    }

    bool setupDirectionalKeys(const uint8_t secret[32], bool isInitiator, const char* channelLabel) {
        uint8_t c2s[32];
        uint8_t s2c[32];
        deriveKeySha256(secret, "c2s_", channelLabel, c2s);
        deriveKeySha256(secret, "s2c_", channelLabel, s2c);

        if (isInitiator) {
            memcpy(txKey_, c2s, 32);
            memcpy(rxKey_, s2c, 32);
            txDirByte_ = 0x01;
            rxDirByte_ = 0x02;
        } else {
            memcpy(txKey_, s2c, 32);
            memcpy(rxKey_, c2s, 32);
            txDirByte_ = 0x02;
            rxDirByte_ = 0x01;
        }

        channelByte_ = (channelLabel && strcmp(channelLabel, "udp") == 0) ? 0x03 :
                       (channelLabel && strcmp(channelLabel, "tcp") == 0) ? 0x02 : 0x01;
        txCounter_ = 0;
        rxCounter_ = 0;

        mbedtls_gcm_free(&gcmTxCtx_);
        mbedtls_gcm_free(&gcmRxCtx_);
        mbedtls_gcm_init(&gcmTxCtx_);
        mbedtls_gcm_init(&gcmRxCtx_);

        return mbedtls_gcm_setkey(&gcmTxCtx_, MBEDTLS_CIPHER_ID_AES, txKey_, 256) == 0 &&
               mbedtls_gcm_setkey(&gcmRxCtx_, MBEDTLS_CIPHER_ID_AES, rxKey_, 256) == 0;
    }
};

} // namespace Connections::UartS