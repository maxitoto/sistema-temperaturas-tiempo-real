import type { auditoria, sensor } from '@prisma/client';
import prisma from '../prismaClient';
import { sensorRepository } from './repositories/sensorRepository';
import { auditoriaRepository } from './repositories/auditoriaRepository';
import { OpcionesLlamada } from '../../communication/layers/CapaAplicacion';
import { firmware } from '../../stubs/clientExample';
import { CredencialRpcDTO } from '../../stubs/contratos';

export type SensorConAuditorias = sensor & { auditorias: auditoria[] };
export type AuditoriaConSensor = auditoria & { sensor: sensor };

export const consultasMixtas = {
    
    getSensorConHistorial: async (id_sensor: number): Promise<SensorConAuditorias | null> =>
        await prisma.sensor.findUnique({
            where: { id_sensor },
            include: { auditorias: { orderBy: { fecha_creacion: 'desc' } } },
        }),

   
    getAuditoriaExacta: async (
        id_sensor: number,
        usuario: string,
        fecha: string
    ): Promise<AuditoriaConSensor | null> =>
        await prisma.auditoria.findFirst({
            where: { id_sensor, usuario, fecha_creacion: fecha },
            include: { sensor: true },
        }),

 
    getHistorialSensorEnRango: async (
        id_sensor: number,
        desde: string,
        hasta: string
    ): Promise<auditoria[]> =>
        await prisma.auditoria.findMany({
            where: { id_sensor, fecha_creacion: { gte: desde, lte: hasta } },
            orderBy: { fecha_creacion: 'desc' },
        }),

   
    getSensorConUltimaAuditoria: async (
        id_sensor: number
    ): Promise<{ sensor: sensor; ultimaAuditoria: auditoria | null } | null> => {
        const sensor = await sensorRepository.getSensorById(id_sensor);
        if (!sensor) return null;
        const ultimaAuditoria = await auditoriaRepository.getUltimaAuditoriaBySensor(id_sensor);
        return { sensor, ultimaAuditoria };
    },

    
    getResumenAuditoriasPorSensor: async (): Promise<
        Array<{ id_sensor: number; nombre: string | null; totalAuditorias: number }>
    > => {
        const rows = await prisma.sensor.findMany({
            where: { activa: 1 },
            select: {
                id_sensor: true,
                nombre: true,
                _count: { select: { auditorias: true } },
            },
            orderBy: { id_sensor: 'asc' },
        });
        return rows.map(r => ({
            id_sensor: r.id_sensor,
            nombre: r.nombre,
            totalAuditorias: r._count.auditorias,
        }));
    },

    
    createSensorConAuditoriaInicial: async (
        dataSensor: Parameters<typeof sensorRepository.createSensor>[0],
        usuario: string
    ): Promise<sensor> =>
        await prisma.$transaction(async (tx) => {
            const nuevo = await tx.sensor.create({ data: dataSensor });
            await tx.auditoria.create({
                data: {
                    id_sensor: nuevo.id_sensor,
                    accion: 'CREATE',
                    usuario,
                },
            });
            return nuevo;
        }),

    obtenerSensoresEnriquecidosConFirmware: async (
    cred: CredencialRpcDTO,
    opts?: OpcionesLlamada
    ) => {
        return await firmware.listarSensoresFirmware(cred, opts);
    }
};