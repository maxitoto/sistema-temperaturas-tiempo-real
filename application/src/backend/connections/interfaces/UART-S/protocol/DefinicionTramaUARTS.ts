export const BYTE_INICIO_TRAMA = 0xAA;
export const BYTE_FIN_TRAMA = 0x55;

export const TAMANO_CABECERA_Y_PIE_TRAMA = 9;
export const TAMANO_MAXIMO_PAYLOAD_ESTATICO = 112;

export enum BanderasControlTrama {
    DATOS          = 0x01,
    ACK            = 0x02,
    NACK           = 0x04,
    MAS_FRAGMENTOS = 0x08,
    CEDER_TURNO_HD = 0x10,
    SINCRONIZAR    = 0x20,
    CERRAR         = 0x40
}

export interface TramaSegmentoUARTS {
    inicioTrama: number;
    banderas: number;
    numeroSecuencia: number;
    numeroConfirmacion: number;
    ventanaRecepcion: number;
    longitudDatos: number;
    datosUtiles: Buffer;
    finTrama: number;
}

export enum EstadoConexionUARTS {
    DESCONECTADO = "DESCONECTADO",
    CONECTANDO = "CONECTANDO",
    CONECTADO = "CONECTADO",
    ERROR = "ERROR"
}