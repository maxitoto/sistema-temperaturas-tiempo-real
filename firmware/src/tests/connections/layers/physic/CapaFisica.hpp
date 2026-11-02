#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ICapaFisica.hpp"

namespace OSI {

    class CapaFisica {
    public:
        static constexpr size_t MAX_RAICES_MEDIOS = 8;

    private:
        ICanalFisico* raices[MAX_RAICES_MEDIOS] = {nullptr};
        size_t        totalRaices               = 0;

    public:
        CapaFisica() = default;

        // Inyecta una raíz (USB, WiFi UDP slot, WiFi TCP, Bluetooth, Mock) y devuelve su idMedio
        int registrarRaiz(ICanalFisico* canal) {
            if (!canal || totalRaices >= MAX_RAICES_MEDIOS) return -1;
            int idAsignado = static_cast<int>(totalRaices);
            raices[totalRaices++] = canal;
            return idAsignado;
        }

        void limpiarRaices() {
            for (size_t i = 0; i < MAX_RAICES_MEDIOS; i++) raices[i] = nullptr;
            totalRaices = 0;
        }

        bool inicializarTodas() {
            bool ok = true;
            for (size_t i = 0; i < totalRaices; i++) {
                if (raices[i] && !raices[i]->inicializar()) ok = false;
            }
            return ok;
        }

        void mantenerTodas() {
            for (size_t i = 0; i < totalRaices; i++) {
                if (raices[i]) raices[i]->mantener();
            }
        }

        int obtenerTotalMedios() const {
            return static_cast<int>(totalRaices);
        }

        ICanalFisico* obtenerRaiz(int idMedio) const {
            if (idMedio < 0 || static_cast<size_t>(idMedio) >= totalRaices) return nullptr;
            return raices[idMedio];
        }

        bool estaActivo(int idMedio) const {
            ICanalFisico* c = obtenerRaiz(idMedio);
            return c ? c->estaActivo() : false;
        }

        bool esMedioCrudo(int idMedio) const {
            ICanalFisico* c = obtenerRaiz(idMedio);
            return c ? c->esMedioCrudo() : true;
        }

        TipoMedio obtenerTipoMedio(int idMedio) const {
            ICanalFisico* c = obtenerRaiz(idMedio);
            return c ? c->obtenerTipo() : TipoMedio::NINGUNO;
        }

        int bytesDisponibles(int idMedio) {
            ICanalFisico* c = obtenerRaiz(idMedio);
            return c ? static_cast<int>(c->bytesDisponibles()) : 0;
        }

        // SUBIDA (RX): Extrae bytes exclusivamente del medio solicitado
        size_t recibirBytes(int idMedio, uint8_t* bufferDestino, size_t maxLongitud) {
            ICanalFisico* c = obtenerRaiz(idMedio);
            if (!c || !c->estaActivo()) return 0;
            return c->recibir(bufferDestino, maxLongitud);
        }

        // BAJADA (TX): Propaga los datos ESTRICTAMENTE por el medio donde vive el cliente
        size_t enviarBytes(int idMedio, const uint8_t* datos, size_t longitud) {
            ICanalFisico* c = obtenerRaiz(idMedio);
            if (!c || !c->estaActivo()) return 0;
            return c->enviar(datos, longitud);
        }

        void desconectarMedio(int idMedio) {
            ICanalFisico* c = obtenerRaiz(idMedio);
            if (c) c->desconectar();
        }
    };

}