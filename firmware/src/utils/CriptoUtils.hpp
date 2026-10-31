// Archivo: src/tests/SQLite/CriptoUtils.hpp
#pragma once
#include <Arduino.h>
#include <vector>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>

class CriptoUtils {
public:
    static String sha256(const String& texto) {
        uint8_t hash[32];
        mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                   reinterpret_cast<const uint8_t*>(texto.c_str()), texto.length(), hash);
        char hex[65];
        for (int i = 0; i < 32; i++) sprintf(hex + (i * 2), "%02x", hash[i]);
        return String(hex);
    }

    static String encriptarAES256(const String& claro, const char* clave32Bytes) {
        size_t len = ((claro.length() + 15) / 16) * 16;
        if (len == 0) len = 16;
        std::vector<uint8_t> in(len, 0), out(len, 0);
        memcpy(in.data(), claro.c_str(), claro.length());

        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_enc(&aes, reinterpret_cast<const uint8_t*>(clave32Bytes), 256);
        for (size_t i = 0; i < len; i += 16)
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, &in[i], &out[i]);
        mbedtls_aes_free(&aes);

        String hex = "";
        char buf[3];
        for (uint8_t b : out) { sprintf(buf, "%02x", b); hex += buf; }
        return hex;
    }

    static String desencriptarAES256(const String& hex, const char* clave32Bytes) {
        size_t len = hex.length() / 2;
        if (len == 0 || len % 16 != 0) return "";
        std::vector<uint8_t> in(len), out(len + 1, 0);
        for (size_t i = 0; i < len; i++)
            in[i] = strtoul(hex.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);

        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_dec(&aes, reinterpret_cast<const uint8_t*>(clave32Bytes), 256);
        for (size_t i = 0; i < len; i += 16)
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, &in[i], &out[i]);
        mbedtls_aes_free(&aes);
        return String(reinterpret_cast<char*>(out.data()));
    }
};