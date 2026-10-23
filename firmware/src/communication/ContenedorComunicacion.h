// Archivo: ./src/communication/ContenedorComunicacion.h
#pragma once
#include "communication/common/BufferRed.h"
#include "communication/layers/CapaFisica.h"
#include "communication/layers/CapaEnlace.h"
#include "communication/layers/CapaRed.h"
#include "communication/layers/CapaTransporte.h"
#include "communication/layers/CapaSesion.h"
#include "communication/layers/CapaPresentacion.h"
#include "communication/layers/CapaAplicacion.h"

class ContenedorComunicacion {
  public:
    CapaFisica fisica;
    CapaRed red;
    CapaTransporte transporte;
    CapaSesion sesion;
    CapaPresentacion presentacion;
    CapaAplicacion appRpc;

    ContenedorComunicacion(PuertoUSB* puertoUsb, ServidorTCP* servidorTcp,
                           const char* secretoHMAC = "SECRETO_HMAC_DEFAULT")
        : fisica(puertoUsb, servidorTcp),
          red(),
          transporte(&red),
          sesion(&transporte, secretoHMAC),
          presentacion(&sesion),
          appRpc(&presentacion) {
        red.vincularMedios(&fisica);
    }

    void procesarCiclo() {
        appRpc.procesar();
    }
};