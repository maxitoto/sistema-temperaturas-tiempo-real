import type { Prisma, sensor } from '@prisma/client';
import prisma from '../../prismaClient';

type SensorCreate = Prisma.sensorUncheckedCreateInput;
type SensorUpdate = Prisma.sensorUncheckedUpdateInput;

export const sensorRepository = {
    getSensores: async (): Promise<sensor[]> =>
        await prisma.sensor.findMany({ orderBy: { id_sensor: 'asc' } }),

    getSensoresActivos: async (): Promise<sensor[]> =>
        await prisma.sensor.findMany({
            where: { activa: 1 },
            orderBy: { id_sensor: 'asc' },
        }),

    getSensorByHexadecimal: async (hexadecimal: string): Promise<sensor | null> =>
        await prisma.sensor.findUnique({ where: { hexadecimal } }),

    getSensorById: async (id_sensor: number): Promise<sensor | null> =>
        await prisma.sensor.findUnique({ where: { id_sensor } }),

    getSensoresByUbicacion: async (ubicacion: string): Promise<sensor[]> =>
        await prisma.sensor.findMany({
            where: { ubicacion, activa: 1 },
            orderBy: { id_sensor: 'asc' },
        }),

    getSensoresByNombre: async (nombre: string): Promise<sensor[]> =>
        await prisma.sensor.findMany({
            where: { nombre: { contains: nombre }, activa: 1 },
            orderBy: { id_sensor: 'asc' },
        }),

    createSensor: async (data: SensorCreate): Promise<sensor> =>
        await prisma.sensor.create({ data }),

    updateSensorById: async (id_sensor: number, data: SensorUpdate): Promise<sensor> =>
        await prisma.sensor.update({ where: { id_sensor }, data }),

    updateSensorByHexadecimal: async (hexadecimal: string, data: SensorUpdate): Promise<sensor> =>
        await prisma.sensor.update({ where: { hexadecimal }, data }),

    setActivaById: async (id_sensor: number, activa: number): Promise<sensor> =>
        await prisma.sensor.update({ where: { id_sensor }, data: { activa } }),

    deleteSensorById: async (id_sensor: number): Promise<sensor> =>
        await prisma.sensor.delete({ where: { id_sensor } }),

    deleteSensorByHexadecimal: async (hexadecimal: string): Promise<sensor> =>
        await prisma.sensor.delete({ where: { hexadecimal } }),

    countSensores: async (): Promise<number> =>
        await prisma.sensor.count(),

    countSensoresActivos: async (): Promise<number> =>
        await prisma.sensor.count({ where: { activa: 1 } }),

    existsById: async (id_sensor: number): Promise<boolean> => {
        const n = await prisma.sensor.count({ where: { id_sensor } });
        return n > 0;
    },

    existsByHexadecimal: async (hexadecimal: string): Promise<boolean> => {
        const n = await prisma.sensor.count({ where: { hexadecimal } });
        return n > 0;
    },
};