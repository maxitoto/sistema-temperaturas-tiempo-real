#pragma once
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

class ServidorTCP {
  private:
    WiFiServer servidor;
    int maxClientes;
    int puerto;
    WiFiClient* clientes;

  public:
    ServidorTCP(int puerto, int limiteClientes) : servidor(puerto), maxClientes(limiteClientes), puerto(puerto) {
        clientes = new WiFiClient[maxClientes];
    }

    ~ServidorTCP() {
        delete[] clientes;
    }

    void iniciar() {
        servidor.begin();
    }

    void detener() {
        servidor.close();
        for (int i = 0; i < maxClientes; i++) {
            if (clientes[i]) clientes[i].stop();
        }
    }

    int obtenerPuerto() const {
        return puerto;
    }

    void procesarNuevasConexiones() {
        if (servidor.hasClient()) {
            bool espacioLibre = false;
            for (int i = 0; i < maxClientes; i++) {
                if (!clientes[i] || !clientes[i].connected()) {
                    if (clientes[i]) clientes[i].stop();
                    clientes[i] = servidor.accept();
                    espacioLibre = true;
                    break;
                }
            }
            if (!espacioLibre) {
                // Rechazar conexión si no hay slots
                WiFiClient clienteRechazado = servidor.accept();
                clienteRechazado.stop();
            }
        }
    }

    WiFiClient* obtenerCliente(int indice) {
        if (indice >= 0 && indice < maxClientes) {
            return &clientes[indice];
        }
        return nullptr;
    }

    int obtenerMaxClientes() const {
        return maxClientes;
    }

    bool hayClientesConectados() const {
        for (int i = 0; i < maxClientes; i++) {
            if (clientes[i] && clientes[i].connected()) {
                return true;
            }
        }
        return false;
    }
};