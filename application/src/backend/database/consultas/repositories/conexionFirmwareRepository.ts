import type { conexion_firmware } from '@prisma/client';
import prisma from '../../prismaClient';

const SINGLETON_ID = 1;

type ConexionCreate = Omit<conexion_firmware, 'id_conexion' | 'fecha_modificacion'>;

export const conexionFirmwareRepository = {

    getConexionFirmware: async (): Promise<conexion_firmware | null> =>
        await prisma.conexion_firmware.findUnique({
            where: { id_conexion: SINGLETON_ID },
        }),

    upsertConexionFirmware: async (data: ConexionCreate): Promise<conexion_firmware> =>
        await prisma.conexion_firmware.upsert({
            where: { id_conexion: SINGLETON_ID },
            update: { ...data, fecha_modificacion: new Date().toISOString() },
            create: { id_conexion: SINGLETON_ID, ...data },
        }),

    setIpDhcp: async (ip_dhcp: string): Promise<conexion_firmware> =>
        await prisma.conexion_firmware.upsert({
            where: { id_conexion: SINGLETON_ID },
            update: { ip_dhcp, fecha_modificacion: new Date().toISOString() },
            create: { id_conexion: SINGLETON_ID, ip_dhcp },
        }),

    setPuertoTcp: async (puerto_tcp: number): Promise<conexion_firmware> =>
        await prisma.conexion_firmware.upsert({
            where: { id_conexion: SINGLETON_ID },
            update: { puerto_tcp, fecha_modificacion: new Date().toISOString() },
            create: { id_conexion: SINGLETON_ID, puerto_tcp },
        }),

    setPuertoUsb: async (puerto_usb: string, baudios_usb: number): Promise<conexion_firmware> =>
        await prisma.conexion_firmware.upsert({
            where: { id_conexion: SINGLETON_ID },
            update: { puerto_usb, baudios_usb, fecha_modificacion: new Date().toISOString() },
            create: { id_conexion: SINGLETON_ID, puerto_usb, baudios_usb },
        }),

    deleteConexionFirmware: async (): Promise<conexion_firmware> =>
        await prisma.conexion_firmware.delete({ where: { id_conexion: SINGLETON_ID } }),
};