import {
    BanderasControlTrama,
    TramaSegmentoUARTS
} from "../protocol/DefinicionTramaUARTS";

export class ReensambladorMensajesUARTS {

    private static readonly CAPACIDAD_BUFFER = 1536;

    private buffer: Buffer = Buffer.alloc(
        ReensambladorMensajesUARTS.CAPACIDAD_BUFFER
    );

    private longitudAcumulada = 0;
    private secuenciaEsperada = 0;
    private mensajeCompleto = false;

    procesarSegmento(
        segmento: TramaSegmentoUARTS
    ): boolean {

        if (this.mensajeCompleto) {
            return false;
        }

        if (segmento.numeroSecuencia === this.secuenciaEsperada) {

            if (
                this.longitudAcumulada + segmento.longitudDatos >
                ReensambladorMensajesUARTS.CAPACIDAD_BUFFER
            ) {
                this.reiniciar();
                return false;
            }

            segmento.datosUtiles.copy(
                this.buffer,
                this.longitudAcumulada
            );

            this.longitudAcumulada += segmento.longitudDatos;

            this.secuenciaEsperada =
                (this.secuenciaEsperada + 1) & 0xFFFF;

            if (
                !(segmento.banderas &
                BanderasControlTrama.MAS_FRAGMENTOS)
            ) {
                this.mensajeCompleto = true;
            }

            return true;
        }

        if (
            ((segmento.numeroSecuencia - this.secuenciaEsperada + 65536)
            & 0xFFFF) > 32768
        ) {
            return true;
        }

        return false;
    }

    hayMensajeCompleto(): boolean {
        return this.mensajeCompleto;
    }

    extraerMensaje(): Buffer | null {
        if (!this.mensajeCompleto) {
            return null;
        }

        const mensaje = Buffer.from(
            this.buffer.subarray(0, this.longitudAcumulada)
        );

        this.buffer.fill(0, 0, this.longitudAcumulada);
        this.longitudAcumulada = 0;
        this.mensajeCompleto = false;

        return mensaje;
    }

    obtenerSecuenciaEsperada(): number {
        return this.secuenciaEsperada;
    }

    calcularVentanaDisponible(
        tamanoMaximoSegmento: number
    ): number {
        if (tamanoMaximoSegmento <= 0) {
            return 0;
        }

        if (this.mensajeCompleto) {
            return Math.min(
                255,
                Math.floor(ReensambladorMensajesUARTS.CAPACIDAD_BUFFER / tamanoMaximoSegmento)
            );
        }

        const bytesLibres =
            ReensambladorMensajesUARTS.CAPACIDAD_BUFFER -
            this.longitudAcumulada;

        return Math.max(
            1,
            Math.min(255, Math.floor(bytesLibres / tamanoMaximoSegmento))
        );
    }

    reiniciar(): void {
        this.buffer.fill(0);
        this.longitudAcumulada = 0;
        this.secuenciaEsperada = 0;
        this.mensajeCompleto = false;
    }
}