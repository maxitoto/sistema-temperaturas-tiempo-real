#pragma once
#include <Arduino.h>
#include "../PrimitivasRTOS.hpp"
#include "../EventosYColas.hpp"
#include "../../hardware/driver/buzz/Buzz.hpp"
#include "../../hardware/driver/buzz/frecuencias.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../database/repositories/RepositorioBase.hpp"

namespace RTOS::Tasks {

    class TaskAlertasYPersistencia {
    private:
        inline static TareaSegura tarea;

        static void procesarColaSonidos() {
            TipoSonidoEvento ev = TipoSonidoEvento::NINGUNO;
            if (!colaSonidos.recibir(ev, 0)) {
                return;
            }

            switch (ev) {
                case TipoSonidoEvento::BOOT_SISTEMA:
                    buzz.sonarBuzz(&sonidoBoot, 0.45f);
                    break;
                case TipoSonidoEvento::CLIENTE_CONECTADO:
                    buzz.sonarBuzz(&sonidoClienteConectado, 0.26f);
                    break;
                case TipoSonidoEvento::CLIENTE_DESCONECTADO:
                    buzz.sonarBuzz(&sonidoClienteDesconectado, 0.26f);
                    break;
                case TipoSonidoEvento::SENSOR_CONECTADO:
                    buzz.sonarBuzz(&sonidoSensorConectado, 0.22f);
                    break;
                case TipoSonidoEvento::SENSOR_DESCONECTADO:
                    buzz.sonarBuzz(&sonidoSensorDesconectado, 0.30f);
                    break;
                case TipoSonidoEvento::TAREA_RPC_EJECUTADA:
                    buzz.sonarBuzz(&sonidoMuteAck, 0.08f);
                    break;
                default:
                    break;
            }
        }

        static void procesarColaPersistenciaDB() {
            RegistroPersistenciaPOD reg{};
            while (colaPersistenciaDB.recibir(reg, 0)) {
                switch (reg.tipo) {
                    case TipoRegistroDB::LOTE_SENSOS_2_MINUTOS: {
                        SnapshotSensor lista[GestorSensores::MAX_SENSORES];
                        uint8_t cant = gestorSensores.obtenerSensoresConectados(lista);
                        if (cant > 0) {
                            DbRuntime::Repositories::TransaccionBatch tx;
                            for (uint8_t i = 0; i < cant; ++i) {
                                // consulta DB (INSERT en tabla Senso para cada sensor conectado en el lote de 2 min)
                                if (lista[i].idSensorDB <= 0) continue;
                                const bool lecturaValida = estadoTieneLecturaValida(lista[i].estado);
                                DbRuntime::Repositories::RepoSenso::registrarLectura(
                                    lista[i].idSensorDB,
                                    lecturaValida ? lista[i].tempCruda     : 0.0f,
                                    lecturaValida ? lista[i].tempCorregida : 0.0f,
                                    lista[i].estado,
                                    static_cast<int64_t>(RelojSistema::obtenerTimestampActual())
                                );
                            }
                            tx.confirmar();
                        }
                        break;
                    }

                    case TipoRegistroDB::EVENTO_CAMBIO_SENSOR: {
                        // 1) SensoDTO s = RepoSenso::registrarLectura(reg.idRelacionado, valida ? reg.tempCruda : 0, valida ? reg.tempCorregida : 0,
                        //      (EstadoSensor)reg.estadoNuevo, reg.timestampUnix);   // valida = estadoTieneLecturaValida(estadoNuevo)
                        // 2) RepoAuditoriaSensores::registrarEvento(reg.idRelacionado, s.idSenso, (EstadoSensor)reg.estadoPrevio,
                        //      (EstadoSensor)reg.estadoNuevo, gestorSensores.obtenerEstadoBus(), tipo, criticidad);
                        // tipo/criticidad: ALARMA_* -> ADVERTENCIA/ALTA | DESCONECTADO, CRC, timeout... -> EXCEPCION/MEDIA
                        //                  CORTOCIRCUITO_*, CIRCUITO_ABIERTO -> EXCEPCION/ALTA | volver a OK -> INFORMATIVO/BAJA
                        // Va en este orden porque AuditoriaSensores.id_senso es FK a Senso.
                        
                        break;
                    }

                    case TipoRegistroDB::EVENTO_SISTEMA_BUS: {
                        // AuditoriaSistemaDTO a; a.estadoBus = (EstadoBus)reg.estadoNuevo; a.detalle = reg.detalle;
                        // a.tipoEventoSistema = (estadoBus==OK) ? INFORMATIVO : (TOPOLOGIA_MODIFICADA ? ADVERTENCIA : EXCEPCION);
                        // a.criticidad según tipo; a.heapLibreBytes = ESP.getFreeHeap(); a.espacioSdLibreKb = (libre de la SD);
                        // a.bootId, a.uptimeS = millis()/1000; a.fuenteTiempo = (la de RelojSistema); a.internet = (WiFi.status()==WL_CONNECTED);
                        // RepoAuditoriaSistema::registrarEvento(a);
                        break;
                    }

                    case TipoRegistroDB::AUDITORIA_TRANSACCION:
                    case TipoRegistroDB::AUDITORIA_RED: {
                        // AuditoriaAuthDTO a; a.idUsuario = reg.idRelacionado; a.origenRemoto = reg.origenOClave; a.accion/detalle = reg.detalle;
                        // a.medioConexion = (según tipo: AUDITORIA_RED -> WIFI_*/UART_S); RepoAuditoriaAuth::registrarEvento(a);
                        break;
                    }
                }
            }
        }

        static void ejecutarCiclo(void*) {
            // 1. Actualizar estado de alarmas continuas (Buzzer + LED) según el estado actual de los 6 sensores
            EstadoSensor estados[GestorSensores::MAX_SENSORES];
            EstadoBus estadoBus = EstadoBus::OK;
            gestorSensores.obtenerEstadosParaAlarma(estados, estadoBus);
            buzz.actualizarAlarmaDesdeSensores(estados, estadoBus);

            // 2. Reproducir tonos cortos de eventos (clientes, sensores en caliente, RPC)
            procesarColaSonidos();

            // 3. Persistir en segundo plano los lotes de 2 minutos, eventos malos y auditorías
            procesarColaPersistenciaDB();
        }

    public:
        static bool iniciar() {
            encolarSonido(TipoSonidoEvento::BOOT_SISTEMA);
            return tarea.iniciarEnNucleo("TaskAlertasDB", &ejecutarCiclo, 100, 4096, 1, 0);
        }
    };

} // namespace RTOS::Tasks