// Archivo: ./src/backend/database/consultas/repositories/conexionFirmwareRepository.ts
import * as fs from "node:fs";
import * as path from "node:path";

export interface ConexionFirmwareJSON {
  id_conexion: number;
  ip_dhcp: string;
  puerto_tcp: number;
  puerto_udp: number;
  puerto_usb: string;
  baudios_usb: number;
  fecha_modificacion: string;
}

type ConexionInput = Partial<Omit<ConexionFirmwareJSON, "id_conexion" | "fecha_modificacion">>;

const rootDir = process.env.APP_ROOT || process.cwd();
const rutaJson = path.resolve(rootDir, "src", "backend", "database", "conexion_firmware.json");

const DEFAULT_CONEXION: ConexionFirmwareJSON = {
  id_conexion: 1,
  ip_dhcp: "192.168.4.1",
  puerto_tcp: 80,
  puerto_udp: 81,
  puerto_usb: "/tmp/virtual_serial",
  baudios_usb: 115200,
  fecha_modificacion: new Date().toISOString(),
};

function leerJsonLocal(): ConexionFirmwareJSON {
  try {
    if (!fs.existsSync(rutaJson)) {
      escribirJsonLocal(DEFAULT_CONEXION);
      return { ...DEFAULT_CONEXION };
    }
    const raw = fs.readFileSync(rutaJson, "utf-8");
    const parsed = JSON.parse(raw) as Partial<ConexionFirmwareJSON>;
    return { ...DEFAULT_CONEXION, ...parsed, id_conexion: 1 };
  } catch {
    return { ...DEFAULT_CONEXION };
  }
}

function escribirJsonLocal(datos: ConexionFirmwareJSON): ConexionFirmwareJSON {
  const dir = path.dirname(rutaJson);
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
  fs.writeFileSync(rutaJson, JSON.stringify(datos, null, 2), "utf-8");
  return datos;
}

export const conexionFirmwareRepository = {
  getConexionFirmware: async (): Promise<ConexionFirmwareJSON> => leerJsonLocal(),

  upsertConexionFirmware: async (data: ConexionInput): Promise<ConexionFirmwareJSON> => {
    const actual = leerJsonLocal();
    return escribirJsonLocal({
      ...actual,
      ...data,
      id_conexion: 1,
      fecha_modificacion: new Date().toISOString(),
    });
  },

  setIpDhcp: async (ip_dhcp: string): Promise<ConexionFirmwareJSON> =>
    conexionFirmwareRepository.upsertConexionFirmware({ ip_dhcp }),

  setPuertoTcp: async (puerto_tcp: number): Promise<ConexionFirmwareJSON> =>
    conexionFirmwareRepository.upsertConexionFirmware({ puerto_tcp }),

  setPuertoUdp: async (puerto_udp: number): Promise<ConexionFirmwareJSON> =>
    conexionFirmwareRepository.upsertConexionFirmware({ puerto_udp }),

  setPuertoUsb: async (puerto_usb: string, baudios_usb: number): Promise<ConexionFirmwareJSON> =>
    conexionFirmwareRepository.upsertConexionFirmware({ puerto_usb, baudios_usb }),
};