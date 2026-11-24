#pragma once
#include <string.h>
#include "../../database/common/PgDtoTypes.hpp"

namespace SmtpNet {

    struct DescripcionDiagnostico {
        const char* titulo;
        const char* significado;
        const char* accion;
    };

    inline DescripcionDiagnostico describirEstadoSensor(EstadoSensor e) {
        switch (e) {
            case EstadoSensor::OK:
                return {
                    "Normalizado (OK)",
                    "La lectura volvio al rango operativo seguro entre TL y TH.",
                    "Sin accion requerida; el equipo continua monitoreando."
                };
            case EstadoSensor::ALARMA_SUPERIOR:
                return {
                    "Temperatura Alta (sobre TH)",
                    "La temperatura corregida supero el limite maximo permitido (TH). Riesgo de perdida de cadena de frio o sobrecalentamiento.",
                    "Verificar cierre de puerta, equipo de refrigeracion y carga termica."
                };
            case EstadoSensor::ALARMA_INFERIOR:
                return {
                    "Temperatura Baja (bajo TL)",
                    "La temperatura corregida cayo por debajo del limite minimo permitido (TL). Riesgo de congelacion o dano por frio.",
                    "Revisar termostato, evaporador y configuracion del umbral TL."
                };
            case EstadoSensor::REINICIO_ENERGIA_85C:
                return {
                    "Reinicio de Energia (+85.0 C)",
                    "El DS18B20 devolvio 85.0 C (valor de encendido), lo que indica que perdio alimentacion o se reinicio durante la medicion.",
                    "Revisar falsos contactos en VCC/GND, borneras y estabilidad de 3.3V."
                };
            case EstadoSensor::DESCONECTADO:
                return {
                    "Sensor Desconectado",
                    "La sonda dejo de responder en el bus 1-Wire durante el sondeo.",
                    "Comprobar conector fisico, cable cortado o retiro manual de la sonda."
                };
            case EstadoSensor::ERROR_CRC_SCRATCHPAD:
                return {
                    "Error CRC de Lectura",
                    "Los datos recibidos llegaron corruptos por ruido electrico o interferencia en la linea.",
                    "Revisar empalmes, alejar cable de motores/220V y verificar pull-up de 4.7 kOhm."
                };
            case EstadoSensor::SCRATCHPAD_VACIO_CEROS:
                return {
                    "Trama Vacia (0x00)",
                    "El sensor devolvio solo ceros; suele indicar linea DATA retenida a masa o falla interna.",
                    "Inspeccionar aislacion del cable de datos y probar la sonda individualmente."
                };
            case EstadoSensor::CORTOCIRCUITO_GND:
                return {
                    "Cortocircuito a Masa (GND)",
                    "La linea de datos esta en corto directo con GND, impidiendo la comunicacion.",
                    "Revisar cables aplastados, humedad en empalmes o sonda en corto."
                };
            case EstadoSensor::CORTOCIRCUITO_VDD:
                return {
                    "Cortocircuito a VCC (VDD)",
                    "La linea de datos quedo pegada a tension positiva (VDD).",
                    "Desconectar la sonda afectada y revisar cableado antes de reconectar."
                };
            case EstadoSensor::CIRCUITO_ABIERTO:
                return {
                    "Circuito Abierto",
                    "No se detecta continuidad electrica ni resistencia pull-up en la linea.",
                    "Verificar conexion al pin 1-Wire y resistencia pull-up de 4.7 kOhm."
                };
            case EstadoSensor::ALIMENTACION_INSUFICIENTE:
                return {
                    "Alimentacion Insuficiente",
                    "La tension en la sonda cayo por debajo del minimo necesario para convertir temperatura.",
                    "Verificar linea VCC (3.3V), evitar modo parasito y revisar fuente."
                };
            case EstadoSensor::TIMEOUT_CONVERSION:
                return {
                    "Timeout de Conversion",
                    "El sensor tardo mas de lo permitido en entregar la medicion ADC.",
                    "Comprobar alimentacion del bus y posible sonda defectuosa."
                };
            case EstadoSensor::CONVERSION_PENDIENTE:
                return {
                    "Conversion Incompleta",
                    "La sonda no habia terminado la conversion interna al momento de leerla.",
                    "Si persiste, revisar fuente de energia y temporizacion del bus."
                };
            case EstadoSensor::DIRECCION_ROM_INVALIDA:
                return {
                    "ROM Invalida (CRC-8)",
                    "La identificacion unica de 64 bits del sensor llego alterada por ruido en el bus.",
                    "Revisar calidad del cableado y longitud total de la linea 1-Wire."
                };
            case EstadoSensor::FAMILIA_NO_COMPATIBLE:
                return {
                    "Dispositivo No Compatible",
                    "Se detecto un chip 1-Wire que no corresponde a la familia DS18B20 (0x28).",
                    "Retirar el dispositivo no compatible del bus."
                };
            case EstadoSensor::FALLO_ESCRITURA_EEPROM:
                return {
                    "Fallo Escritura EEPROM",
                    "No se pudo guardar la configuracion interna en la memoria del sensor.",
                    "Reintentar configuracion; si persiste, reemplazar la sonda."
                };
            default:
                return {
                    "Estado Anomalo de Sensor",
                    "La sonda reporto una condicion fuera de operacion normal.",
                    "Revisar el detalle del sensor desde la aplicacion de escritorio."
                };
        }
    }

    inline DescripcionDiagnostico describirEstadoBus(EstadoBus b) {
        switch (b) {
            case EstadoBus::OK:
                return {
                    "Bus 1-Wire Operativo",
                    "La linea de sensores responde correctamente a los pulsos de presencia.",
                    "Sin accion requerida."
                };
            case EstadoBus::LINEA_EN_CORTO_GND:
                return {
                    "Bus en Cortocircuito a GND",
                    "Toda la linea 1-Wire quedo clavada a 0V; ningun sensor puede comunicar.",
                    "Desconectar sondas una por una hasta aislar el cable o sonda en corto."
                };
            case EstadoBus::SIN_PULSO_PRESENCIA:
                return {
                    "Sin Sensores en Bus",
                    "Ninguna sonda respondio al pulso de reset 1-Wire.",
                    "Verificar cable troncal, conectores y alimentacion de los sensores."
                };
            case EstadoBus::TOPOLOGIA_MODIFICADA:
                return {
                    "Cambio de Sensores en Caliente",
                    "Se detecto la conexion o desconexion fisica de una o mas sondas en el bus.",
                    "Confirmar si fue una intervencion planificada en el equipo."
                };
            case EstadoBus::FALLO_CONVERSION_GLOBAL:
                return {
                    "Fallo de Conversion Global",
                    "Fallo la orden simultanea de medicion para todas las sondas del bus 1-Wire.",
                    "Revisar alimentacion compartida de 3.3V y resistencia pull-up de 4.7 kOhm."
                };
            case EstadoBus::LIMITE_SENSORES_EXCEDIDO:
                return {
                    "Limite de Sensores Excedido",
                    "Hay mas sondas fisicas conectadas que el maximo de 6 slots soportados.",
                    "Desconectar las sondas excedentes y re-escanear el bus."
                };
            default:
                return {
                    "Anomalia en Bus 1-Wire",
                    "El bus de sensores presento un estado inesperado.",
                    "Inspeccionar el cableado principal del bus."
                };
        }
    }

    inline const char* explicarMotivoReset(const char* reason) {
        if (!reason || reason[0] == '\0') return "Sin detalle de reinicio";
        if (strcmp(reason, "POWERON_RESET") == 0)   return "Encendido normal o retorno tras corte electrico";
        if (strcmp(reason, "BROWNOUT") == 0)        return "Caida brusca de voltaje en la fuente de alimentacion";
        if (strcmp(reason, "TASK_WDT") == 0 || strcmp(reason, "INT_WDT") == 0 || strcmp(reason, "OTHER_WDT") == 0)
            return "Recuperacion automatica por Watchdog ante bloqueo de tarea";
        if (strcmp(reason, "EXCEPTION_PANIC") == 0) return "Reinicio por excepcion critica de memoria/CPU";
        if (strcmp(reason, "SW_RESTART") == 0)      return "Reinicio solicitado por software o administrador";
        if (strcmp(reason, "DEEPSLEEP") == 0)       return "Despertar desde modo apagado seguro";
        return reason;
    }

} // namespace SmtpNet