// En SerializadorTramasUARTS.ts
import {
    BYTE_INICIO_TRAMA,
    BYTE_FIN_TRAMA,
    TAMANO_CABECERA_Y_PIE_TRAMA,
    TAMANO_MAXIMO_PAYLOAD_ESTATICO,
    TramaSegmentoUARTS
} from "./DefinicionTramaUARTS";

export class SerializadorTramasUARTS {
    static serializar(trama: TramaSegmentoUARTS): Buffer {
        if (
            trama.longitudDatos < 0 ||
            trama.longitudDatos > TAMANO_MAXIMO_PAYLOAD_ESTATICO ||
            trama.datosUtiles.length !== trama.longitudDatos
        ) {
            throw new Error("Longitud de payload inválida.");
        }

        const buffer = Buffer.alloc(
            TAMANO_CABECERA_Y_PIE_TRAMA + trama.longitudDatos
        );

        let offset = 0;

        buffer.writeUInt8(BYTE_INICIO_TRAMA, offset++);
        buffer.writeUInt8(trama.banderas, offset++);

        buffer.writeUInt16BE(trama.numeroSecuencia & 0xFFFF, offset);
        offset += 2;

        buffer.writeUInt16BE(trama.numeroConfirmacion & 0xFFFF, offset);
        offset += 2;

        buffer.writeUInt8(trama.ventanaRecepcion, offset++);
        buffer.writeUInt8(trama.longitudDatos, offset++);

        if (trama.longitudDatos > 0) {
            trama.datosUtiles.copy(buffer, offset);
            offset += trama.longitudDatos;
        }

        buffer.writeUInt8(BYTE_FIN_TRAMA, offset);

        return buffer;
    }

    static crearTramaControl(
        banderas: number,
        numeroConfirmacion: number,
        ventanaRecepcion: number
    ): TramaSegmentoUARTS {
        return {
            inicioTrama: BYTE_INICIO_TRAMA,
            banderas,
            numeroSecuencia: 0,
            numeroConfirmacion,
            ventanaRecepcion,
            longitudDatos: 0,
            datosUtiles: Buffer.alloc(0),
            finTrama: BYTE_FIN_TRAMA
        };
    }
}