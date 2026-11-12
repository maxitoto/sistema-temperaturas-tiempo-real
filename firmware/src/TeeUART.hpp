// TeeUART.hpp
#pragma once
#include <Arduino.h>

class TeeUART : public Stream {
public:
    TeeUART(HardwareSerial& uart, Print& out) : uart_(uart), out_(out) {}

    HardwareSerial& hardwareSerial() { return uart_; }
    Print* echoOutput() {
        return (echo_ && static_cast<const Print*>(&uart_) != &out_) ? &out_ : nullptr;
    }

    void begin(unsigned long baud, uint32_t cfg = SERIAL_8N1,
               int8_t rx = -1, int8_t tx = -1) {
        uart_.begin(baud, cfg, rx, tx);
    }

    void setEcho(bool on) { echo_ = on; }

    size_t write(uint8_t b) override {
        if (auto* o = echoOutput()) { o->write('>'); o->write(b); }
        return uart_.write(b);
    }
    size_t write(const uint8_t* buf, size_t len) override {
        if (auto* o = echoOutput()) {
            o->write('>');
            o->write(buf, len);
        }
        return uart_.write(buf, len);
    }

    int available() override { return uart_.available(); }
    int read() override {
        int b = uart_.read();
        if (b >= 0) {
            if (auto* o = echoOutput()) { o->write('<'); o->write(static_cast<uint8_t>(b)); }
        }
        return b;
    }
    int peek() override { return uart_.peek(); }
    void flush() override { uart_.flush(); }
    operator bool() const { return static_cast<bool>(uart_); }

private:
    HardwareSerial& uart_;
    Print&          out_;
    bool            echo_ = true;
};