import {
    BYTE_INICIO_TRAMA,
    BYTE_FIN_TRAMA,
    TAMANO_MAXIMO_PAYLOAD_ESTATICO,
    BanderasControlTrama,
    TramaSegmentoUARTS
} from "../protocol/DefinicionTramaUARTS";

export class SegmentadorMensajesUARTS {

    static dividirMensaje(
        mensaje: Buffer,
        numeroSecuenciaInicial: number,
        ultimoAckRecibido: number,
        ventanaRecepcion: number,
        tamanoMaximoPayload: number = TAMANO_MAXIMO_PAYLOAD_ESTATICO
    ): TramaSegmentoUARTS[] {

        if (mensaje.length === 0) {
            return [];
        }

        const tamanoFragmento = Math.min(
            tamanoMaximoPayload,
            TAMANO_MAXIMO_PAYLOAD_ESTATICO
        );

        if (tamanoFragmento <= 0) {
            return [];
        }

        const segmentos: TramaSegmentoUARTS[] = [];

        let offset = 0;
        let secuencia = numeroSecuenciaInicial;

        while (offset < mensaje.length) {

            const longitud = Math.min(
                mensaje.length - offset,
                tamanoFragmento
            );

            const hayMasFragmentos =
                offset + longitud < mensaje.length;

            let banderas = BanderasControlTrama.DATOS;

            if (hayMasFragmentos) {
                banderas |= BanderasControlTrama.MAS_FRAGMENTOS;
            }

            segmentos.push({
                inicioTrama: BYTE_INICIO_TRAMA,
                banderas,
                numeroSecuencia: secuencia & 0xFFFF,
                numeroConfirmacion: ultimoAckRecibido & 0xFFFF,
                ventanaRecepcion,
                longitudDatos: longitud,
                datosUtiles: Buffer.from(
                    mensaje.subarray(offset, offset + longitud)
                ),
                finTrama: BYTE_FIN_TRAMA
            });

            secuencia = (secuencia + 1) & 0xFFFF;
            offset += longitud;
        }

        return segmentos;
    }
}