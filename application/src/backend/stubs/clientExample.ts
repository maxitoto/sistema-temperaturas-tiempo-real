// Archivo: ./src/backend/stubs/clientExample.ts
import { pila } from "../communication/ContenedorComunicacion";
import { OpcionesLlamada } from "../communication/layers/CapaAplicacion";
import {
  IFirmware,
  RPC,
  RespuestaCalculoFirmware,
  SolicitudConfigSeguraDTO,
  RespuestaAuditoriaFirmwareDTO,
} from "./contratos";

// =========================================================================
// 1. CRIPTOGRAFÍA SIMÉTRICA DE CANAL (CAPA 6) Y DE CAMPO (CAPA 7)
// =========================================================================
const CLAVE_CANAL_TLS = new Uint8Array([
  0x4a, 0x91, 0x2c, 0xe7, 0x55, 0xb3, 0x18, 0x8f,
  0xd2, 0x3e, 0x7a, 0xc1, 0x60, 0x09, 0xf4, 0x33,
]);

const CLAVE_CAMPO_SMTP = new Uint8Array([
  0x73, 0x6d, 0x74, 0x70, 0x5f, 0x73, 0x65, 0x63, 0x5f, 0x6b, 0x65, 0x79,
]);

export function transformarMensajeCanalTLS(
  _idNodo: number,
  _reqId: number,
  vista: Uint8Array
): boolean {
  for (let i = 0; i < vista.length; i++) {
    vista[i] ^= (CLAVE_CANAL_TLS[i & 0x0f] ^ ((i * 29) & 0xff)) & 0xff;
  }
  return true;
}

export function encriptarCampoSensible(textoPlano: string): Uint8Array {
  const bytes = Buffer.from(textoPlano, "utf-8");
  const cifrado = new Uint8Array(bytes.length);
  for (let i = 0; i < bytes.length; i++) {
    cifrado[i] =
      (bytes[i] ^ CLAVE_CAMPO_SMTP[i % CLAVE_CAMPO_SMTP.length] ^ ((i * 7) & 0xff)) & 0xff;
  }
  return cifrado;
}

// =========================================================================
// 2. STUBS CLIENTE (CAPA 7)
// =========================================================================
// STUB CLIENTE: implementa el contrato IFirmware llamando por red
// =========================================================================
export class FirmwareStub implements IFirmware {
  async sumarYLeerEstado(
    base: number,
    factor: number,
    opts?: OpcionesLlamada
  ): Promise<RespuestaCalculoFirmware> {
    const [resultado, estado, heapLibreBytes] = await pila.appRpc.call<[number, string, number]>(
      RPC.SUMAR_Y_ESTADO,
      [Number(base), Number(factor)],
      opts
    );
    return {
      resultado: Number(resultado),
      estado: String(estado),
      heapLibreBytes: Number(heapLibreBytes),
    };
  }

  async guardarConfiguracionSegura(
    dto: SolicitudConfigSeguraDTO,
    opts?: OpcionesLlamada
  ): Promise<RespuestaAuditoriaFirmwareDTO> {
    const smtpCifradaBin = encriptarCampoSensible(dto.smtpPasswordPlana);

    const [
      idRegistro, tempMinGuardada, tempMaxGuardada, smtpDesencriptadaEnFirmware,
      modificadoPor, rolUsuario, medioUsado, totalFilasEnDB,
    ] = await pila.appRpc.call<[number, number, number, string, string, string, string, number]>(
      RPC.GUARDAR_CONFIG_SEGURA,
      [
        dto.emisorCertificado,
        dto.firmaCertificado,
        dto.username,
        dto.pinHash,
        Number(dto.tempMin),
        Number(dto.tempMax),
        smtpCifradaBin,
      ],
      opts
    );

    return {
      idRegistro: Number(idRegistro),
      tempMinGuardada: Number(tempMinGuardada),
      tempMaxGuardada: Number(tempMaxGuardada),
      smtpDesencriptadaEnFirmware: String(smtpDesencriptadaEnFirmware),
      modificadoPor: String(modificadoPor),
      rolUsuario: String(rolUsuario),
      medioUsado: String(medioUsado),
      totalFilasEnDB: Number(totalFilasEnDB),
      hexCampoCifradoEnviado: Buffer.from(smtpCifradaBin).toString("hex"),
    };
  }
}

export const firmware: IFirmware = new FirmwareStub();