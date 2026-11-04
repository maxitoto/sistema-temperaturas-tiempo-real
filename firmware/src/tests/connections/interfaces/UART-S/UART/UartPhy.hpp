#pragma once
#include <HardwareSerial.h>
#include <vector>
#include <stdint.h>

namespace Connections::UartS {

class UartPhy {
public:
    // Metadatos físicos hardcodeados (invariantes del firmware)
    static constexpr uint32_t BAUD_RATE = 115200;
    static constexpr int8_t DEFAULT_RX_PIN = -1; // -1 conserva los pines nativos del UART seleccionado
    static constexpr int8_t DEFAULT_TX_PIN = -1;
    static constexpr size_t RX_BUFFER_SIZE = 2048;
    static constexpr size_t MAX_FRAME_BUFFER = 512;

    explicit UartPhy(HardwareSerial& serial) : serial_(serial) {}

    void loadConfig() {
        // Sin pines ni baudios en BD: son invariantes físicos hardcodeados
    }

    bool begin() {
        serial_.setRxBufferSize(RX_BUFFER_SIZE);
        if (DEFAULT_RX_PIN >= 0 && DEFAULT_TX_PIN >= 0) {
            serial_.begin(BAUD_RATE, SERIAL_8N1, DEFAULT_RX_PIN, DEFAULT_TX_PIN);
        } else {
            serial_.begin(BAUD_RATE);
        }
        discardBootloaderGarbage();
        rxAccum_.clear();
        return true;
    }

    void sendFrame(const uint8_t* data, size_t len) {
        std::vector<uint8_t> encoded = cobsEncode(data, len);
        uint8_t delimiter = 0x00;
        serial_.write(&delimiter, 1);
        serial_.write(encoded.data(), encoded.size());
        serial_.write(&delimiter, 1);
    }

    bool pollFrame(std::vector<uint8_t>& outDecodedFrame) {
        while (serial_.available()) {
            uint8_t b = static_cast<uint8_t>(serial_.read());
            if (b == 0x00) {
                if (!rxAccum_.empty()) {
                    bool ok = cobsDecode(rxAccum_.data(), rxAccum_.size(), outDecodedFrame);
                    rxAccum_.clear();
                    if (ok) return true;
                }
            } else {
                if (rxAccum_.size() < MAX_FRAME_BUFFER) {
                    rxAccum_.push_back(b);
                } else {
                    rxAccum_.clear();
                }
            }
        }
        return false;
    }

private:
    HardwareSerial& serial_;
    std::vector<uint8_t> rxAccum_;

    void discardBootloaderGarbage() {
        uint32_t start = millis();
        while (millis() - start < 50) {
            while (serial_.available()) serial_.read();
        }
    }

    static std::vector<uint8_t> cobsEncode(const uint8_t* input, size_t length) {
        std::vector<uint8_t> output;
        output.reserve(length + 2);
        output.push_back(0);
        size_t code_index = 0;
        uint8_t code = 1;

        for (size_t i = 0; i < length; ++i) {
            if (input[i] == 0) {
                output[code_index] = code;
                code = 1;
                code_index = output.size();
                output.push_back(0);
            } else {
                output.push_back(input[i]);
                if (++code == 0xFF) {
                    output[code_index] = code;
                    code = 1;
                    code_index = output.size();
                    output.push_back(0);
                }
            }
        }
        output[code_index] = code;
        return output;
    }

    static bool cobsDecode(const uint8_t* input, size_t length, std::vector<uint8_t>& output) {
        output.clear();
        size_t i = 0;
        while (i < length) {
            uint8_t code = input[i++];
            if (code == 0 || (i + code - 1 > length)) return false;
            for (uint8_t j = 1; j < code; ++j) {
                output.push_back(input[i++]);
            }
            if (code < 0xFF && i < length) {
                output.push_back(0);
            }
        }
        return true;
    }
};

} // namespace Connections::UartS