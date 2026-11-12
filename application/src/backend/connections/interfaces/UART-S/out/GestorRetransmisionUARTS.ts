import {
    BanderasControlTrama,
    TramaSegmentoUARTS
} from "../protocol/DefinicionTramaUARTS";

import { SerializadorTramasUARTS } from "../protocol/SerializadorTramasUARTS";

export interface TransporteUARTS {
    escribir(datos: Buffer): boolean;
}

export interface TransporteUARTS {
    escribir(datos: Buffer): boolean;
    onFalloEnlace?: () => void;
}

interface SegmentoPendiente {
    trama: TramaSegmentoUARTS;
    enviado: boolean;
    ultimoEnvio: number;
    intentos: number;
}

export class GestorRetransmisionUARTS {

    private static readonly CAPACIDAD_COLA = 16;
    private static readonly MAXIMO_INTENTOS = 5;
    private static readonly RTO_MS = 250;

    private cola: SegmentoPendiente[] = [];

    private ventanaRemota = 16;

    constructor(
        private readonly transporte: TransporteUARTS
    ) {}

    encolar(trama: TramaSegmentoUARTS): boolean {

        if (this.cola.length >= GestorRetransmisionUARTS.CAPACIDAD_COLA) {
            return false;
        }

        this.cola.push({
            trama,
            enviado: false,
            ultimoEnvio: 0,
            intentos: 0
        });

        return true;
    }

    enviarControl(
        banderas: number,
        numeroConfirmacion: number,
        ventanaLocal: number
    ): boolean {

        const trama = SerializadorTramasUARTS.crearTramaControl(
            banderas,
            numeroConfirmacion,
            ventanaLocal
        );

        return this.transporte.escribir(
            SerializadorTramasUARTS.serializar(trama)
        );
    }

    procesarConfirmacion(
        banderas: number,
        numeroConfirmacion: number,
        ventanaRemota: number
    ): void {
        this.ventanaRemota = Math.max(1, ventanaRemota);

        if (banderas & BanderasControlTrama.ACK) {

            this.cola = this.cola.filter(
                segmento =>
                    segmento.trama.numeroSecuencia !== numeroConfirmacion
            );

        } else if (banderas & BanderasControlTrama.NACK) {

            const segmento = this.cola.find(
                item =>
                    item.trama.numeroSecuencia === numeroConfirmacion
            );

            if (segmento) {
                segmento.ultimoEnvio = 0;
            }
        }
    }

    ejecutarCiclo(): void {

        const ahora = Date.now();

        const enVuelo = this.cola.filter(
            segmento => segmento.enviado
        ).length;

        for (const segmento of this.cola) {

            if (!segmento.enviado) {
                continue;
            }

            if (
                ahora - segmento.ultimoEnvio >=
                GestorRetransmisionUARTS.RTO_MS
            ) {

                if (
                    segmento.intentos >=
                    GestorRetransmisionUARTS.MAXIMO_INTENTOS
                ) {
                    this.cola = [];
                    this.transporte.onFalloEnlace?.();
                    return;
                }
                
                if (this.transporte.escribir(
                    SerializadorTramasUARTS.serializar(segmento.trama)
                )) {
                    segmento.ultimoEnvio = ahora;
                    segmento.intentos++;
                }

                return;
            }
        }

        if (enVuelo >= Math.min(this.ventanaRemota, 2)) {
            return;
        }

        const siguiente = this.cola.find(
            segmento => !segmento.enviado
        );

        if (!siguiente) {
            return;
        }

        if (this.transporte.escribir(
            SerializadorTramasUARTS.serializar(siguiente.trama)
        )) {
            siguiente.enviado = true;
            siguiente.ultimoEnvio = ahora;
            siguiente.intentos = 1;
        }
    }

    reiniciar(): void {
        this.cola = [];
        this.ventanaRemota = 16;
    }
}