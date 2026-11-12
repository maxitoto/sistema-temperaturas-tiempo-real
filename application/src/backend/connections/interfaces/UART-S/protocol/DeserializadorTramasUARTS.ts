// En DeserializadorTramasUARTS.ts
import {
    BYTE_INICIO_TRAMA,
    BYTE_FIN_TRAMA,
    TAMANO_MAXIMO_PAYLOAD_ESTATICO,
    TAMANO_CABECERA_Y_PIE_TRAMA,
    TramaSegmentoUARTS
} from "./DefinicionTramaUARTS";

export class DeserializadorTramasUARTS {
    private bufferAcumulado: Buffer = Buffer.alloc(0);

    procesarBytes(bytes: Buffer): TramaSegmentoUARTS[] {
        this.bufferAcumulado = Buffer.concat([this.bufferAcumulado, bytes]);
        const tramas: TramaSegmentoUARTS[] = [];

        while (this.bufferAcumulado.length >= TAMANO_CABECERA_Y_PIE_TRAMA) {
            const inicio = this.bufferAcumulado.indexOf(BYTE_INICIO_TRAMA);

            if (inicio === -1) {
                this.bufferAcumulado = Buffer.alloc(0);
                break;
            }

            if (inicio > 0) {
                this.bufferAcumulado = this.bufferAcumulado.subarray(inicio);
            }

            if (this.bufferAcumulado.length < TAMANO_CABECERA_Y_PIE_TRAMA) {
                break;
            }

            const longitudDatos = this.bufferAcumulado.readUInt8(7);

            if (longitudDatos > TAMANO_MAXIMO_PAYLOAD_ESTATICO) {
                this.bufferAcumulado = this.bufferAcumulado.subarray(1);
                continue;
            }

            const longitudTrama = TAMANO_CABECERA_Y_PIE_TRAMA + longitudDatos;

            if (this.bufferAcumulado.length < longitudTrama) {
                break;
            }

            const fin = this.bufferAcumulado.readUInt8(longitudTrama - 1);

            if (fin !== BYTE_FIN_TRAMA) {
                this.bufferAcumulado = this.bufferAcumulado.subarray(1);
                continue;
            }

            const trama: TramaSegmentoUARTS = {
                inicioTrama: BYTE_INICIO_TRAMA,
                banderas: this.bufferAcumulado.readUInt8(1),
                numeroSecuencia: this.bufferAcumulado.readUInt16BE(2),
                numeroConfirmacion: this.bufferAcumulado.readUInt16BE(4),
                ventanaRecepcion: this.bufferAcumulado.readUInt8(6),
                longitudDatos,
                datosUtiles: Buffer.from(
                    this.bufferAcumulado.subarray(8, 8 + longitudDatos)
                ),
                finTrama: BYTE_FIN_TRAMA
            };

            tramas.push(trama);
            this.bufferAcumulado = this.bufferAcumulado.subarray(longitudTrama);
        }

        return tramas;
    }

    reiniciar(): void {
        this.bufferAcumulado = Buffer.alloc(0);
    }
}