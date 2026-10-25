// Archivo: ./src/backend/stubs/clientExample.ts
import { pila } from "../communication/ContenedorComunicacion";
import { OpcionesLlamada } from "../communication/layers/CapaAplicacion";
import {
  IFirmware,
  RPC,
  CredencialRpcDTO,
  RespuestaCalculoFirmware,
  SolicitudConfigSeguraDTO,
  RespuestaAuditoriaFirmwareDTO,
  SensorFirmwareDTO,
  GuardarSensorFirmwareInput,
  SensoFirmwareDTO,
  EventoSensorFirmwareDTO,
  AuditoriaFirmwareRowDTO,
  CorreoFirmwareDTO,
  CrearCorreoFirmwareInput,
  ConfigSmtpFirmwareResumenDTO,
  ConfigRedYNtpFirmwareDTO,
  GuardarRedFirmwareInput,
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

function serializarCredencial(cred: CredencialRpcDTO): [string, string, string, string] {
  return [
    cred.emisorCertificado ?? "CA-UNTDF-LAB",
    cred.firmaCertificado ?? "CERT-TLS-UNTDF-2026-VALIDO",
    cred.username,
    cred.pinHash,
  ];
}

type TuplaSensor = [number, string, number, number, number, number, number, number, number];
type TuplaSenso = [number, number, number, number, number, number];
type TuplaEvento = [number, number, number, number, number, number, number];
type TuplaAuditoria = [number, number, string, string, number];
type TuplaCorreo = [number, string, string, string, number, number, number];

function mapearTuplaSensor(t: TuplaSensor): SensorFirmwareDTO {
  return {
    idSensor: Number(t[0]),
    hexadecimal: String(t[1]),
    errorSuperior: Number(t[2]),
    errorInferior: Number(t[3]),
    umbralSuperior: Number(t[4]),
    umbralInferior: Number(t[5]),
    activa: Number(t[6]),
    fechaCreacion: Number(t[7]),
    fechaModificacion: Number(t[8]),
  };
}

// =========================================================================
// 2. STUB CLIENTE RPC: Implementa IFirmware con propiedades enumerables
// =========================================================================
export class FirmwareStub implements IFirmware {
  public readonly sumarYLeerEstado = async (
    base: number,
    factor: number,
    opts?: OpcionesLlamada
  ): Promise<RespuestaCalculoFirmware> => {
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
  };

  public readonly guardarConfiguracionSegura = async (
    dto: SolicitudConfigSeguraDTO,
    opts?: OpcionesLlamada
  ): Promise<RespuestaAuditoriaFirmwareDTO> => {
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
  };

  public readonly listarSensoresFirmware = async (
    cred: CredencialRpcDTO,
    opts?: OpcionesLlamada
  ): Promise<SensorFirmwareDTO[]> => {
    const [filas] = await pila.appRpc.call<[TuplaSensor[]]>(
      RPC.DB_LISTAR_SENSORES,
      [...serializarCredencial(cred)],
      opts
    );
    return (filas ?? []).map(mapearTuplaSensor);
  };

  public readonly guardarSensorFirmware = async (
    input: GuardarSensorFirmwareInput,
    opts?: OpcionesLlamada
  ): Promise<SensorFirmwareDTO> => {
    const [tupla] = await pila.appRpc.call<[TuplaSensor]>(
      RPC.DB_GUARDAR_SENSOR,
      [
        ...serializarCredencial(input.credencial),
        input.hexadecimal,
        Number(input.errorSuperior),
        Number(input.errorInferior),
        Number(input.umbralSuperior),
        Number(input.umbralInferior),
      ],
      opts
    );
    return mapearTuplaSensor(tupla);
  };

  public readonly listarUltimosSensosFirmware = async (
    cred: CredencialRpcDTO,
    idSensor = 0,
    limite = 8,
    opts?: OpcionesLlamada
  ): Promise<SensoFirmwareDTO[]> => {
    const [filas] = await pila.appRpc.call<[TuplaSenso[]]>(
      RPC.DB_ULTIMOS_SENSOS,
      [...serializarCredencial(cred), Number(idSensor), Number(limite)],
      opts
    );
    return (filas ?? []).map((t) => ({
      idSenso: Number(t[0]),
      idSensor: Number(t[1]),
      temperaturaCruda: Number(t[2]),
      temperaturaCorregida: Number(t[3]),
      estadoSensor: Number(t[4]),
      fechaCreacion: Number(t[5]),
    }));
  };

  public readonly listarUltimosEventosFirmware = async (
    cred: CredencialRpcDTO,
    idSensor = 0,
    limite = 8,
    opts?: OpcionesLlamada
  ): Promise<EventoSensorFirmwareDTO[]> => {
    const [filas] = await pila.appRpc.call<[TuplaEvento[]]>(
      RPC.DB_ULTIMOS_EVENTOS,
      [...serializarCredencial(cred), Number(idSensor), Number(limite)],
      opts
    );
    return (filas ?? []).map((t) => ({
      idEventoSensor: Number(t[0]),
      idSensor: Number(t[1]),
      idSenso: Number(t[2]),
      estadoPrevioSensor: Number(t[3]),
      evento: Number(t[4]),
      criticidad: Number(t[5]),
      fechaCreacion: Number(t[6]),
    }));
  };

  public readonly listarUltimasAuditoriasFirmware = async (
    cred: CredencialRpcDTO,
    limite = 5,
    opts?: OpcionesLlamada
  ): Promise<AuditoriaFirmwareRowDTO[]> => {
    const [filas] = await pila.appRpc.call<[TuplaAuditoria[]]>(
      RPC.DB_ULTIMAS_AUDITORIAS,
      [...serializarCredencial(cred), Number(limite)],
      opts
    );
    return (filas ?? []).map((t) => ({
      idAuditoria: Number(t[0]),
      idUsuario: Number(t[1]),
      accion: String(t[2]),
      detalle: String(t[3]),
      fechaCreacion: Number(t[4]),
    }));
  };

  public readonly listarCorreosFirmware = async (
    cred: CredencialRpcDTO,
    opts?: OpcionesLlamada
  ): Promise<CorreoFirmwareDTO[]> => {
    const [filas] = await pila.appRpc.call<[TuplaCorreo[]]>(
      RPC.DB_LISTAR_CORREOS,
      [...serializarCredencial(cred)],
      opts
    );
    return (filas ?? []).map((t) => ({
      idCorreo: Number(t[0]),
      nombre: String(t[1]),
      apellido: String(t[2]),
      correoElectronico: String(t[3]),
      tipoRecepcionMensajes: Number(t[4]),
      estadoEnvio: Number(t[5]),
      fechaModificacion: Number(t[6]),
    }));
  };

  public readonly crearCorreoFirmware = async (
    input: CrearCorreoFirmwareInput,
    opts?: OpcionesLlamada
  ): Promise<CorreoFirmwareDTO> => {
    const [t] = await pila.appRpc.call<[TuplaCorreo]>(
      RPC.DB_CREAR_CORREO,
      [
        ...serializarCredencial(input.credencial),
        input.nombre,
        input.apellido ?? "",
        input.correoElectronico,
        Number(input.tipoRecepcionMensajes ?? 0),
      ],
      opts
    );
    return {
      idCorreo: Number(t[0]),
      nombre: String(t[1]),
      apellido: String(t[2]),
      correoElectronico: String(t[3]),
      tipoRecepcionMensajes: Number(t[4]),
      estadoEnvio: Number(t[5]),
      fechaModificacion: Number(t[6]),
    };
  };

  public readonly obtenerConfigSmtpFirmware = async (
    cred: CredencialRpcDTO,
    opts?: OpcionesLlamada
  ): Promise<ConfigSmtpFirmwareResumenDTO> => {
    const [configurado, hostSmtp, puerto, usuario, emailRemitente, fechaModificacion] =
      await pila.appRpc.call<[boolean, string, number, string, string, number]>(
        RPC.DB_OBTENER_SMTP,
        [...serializarCredencial(cred)],
        opts
      );
    return {
      configurado: Boolean(configurado),
      hostSmtp: String(hostSmtp),
      puerto: Number(puerto),
      usuario: String(usuario),
      emailRemitente: String(emailRemitente),
      fechaModificacion: Number(fechaModificacion),
    };
  };

  public readonly obtenerConfigRedYNtpFirmware = async (
    cred: CredencialRpcDTO,
    opts?: OpcionesLlamada
  ): Promise<ConfigRedYNtpFirmwareDTO> => {
    const [ssidSta, ssidAp, fechaModificacion, servidoresNtp] = await pila.appRpc.call<
      [string, string, number, string[]]
    >(RPC.DB_OBTENER_RED_Y_NTP, [...serializarCredencial(cred)], opts);

    return {
      ssidSta: String(ssidSta),
      ssidAp: String(ssidAp),
      fechaModificacion: Number(fechaModificacion),
      servidoresNtp: (servidoresNtp ?? []).map(String),
    };
  };

  public readonly guardarConfigRedFirmware = async (
    input: GuardarRedFirmwareInput,
    opts?: OpcionesLlamada
  ): Promise<{ ssidSta: string; ssidAp: string; fechaModificacion: number }> => {
    const passStaBin = encriptarCampoSensible(input.passwordStaPlana);
    const passApBin = encriptarCampoSensible(input.passwordApPlana);

    const [ssidSta, ssidAp, fechaModificacion] = await pila.appRpc.call<[string, string, number]>(
      RPC.DB_GUARDAR_RED,
      [
        ...serializarCredencial(input.credencial),
        input.ssidSta,
        passStaBin,
        input.ssidAp,
        passApBin,
      ],
      opts
    );

    return {
      ssidSta: String(ssidSta),
      ssidAp: String(ssidAp),
      fechaModificacion: Number(fechaModificacion),
    };
  };
}

export const firmware: IFirmware = new FirmwareStub();