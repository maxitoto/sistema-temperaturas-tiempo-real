import type { Prisma, auditoria } from '@prisma/client';
import prisma from '../../prismaClient';

type AuditoriaCreate = Prisma.auditoriaUncheckedCreateInput;

export const auditoriaRepository = {
    
    getAuditoria: async (): Promise<auditoria[]> =>
        await prisma.auditoria.findMany({ orderBy: { fecha_creacion: 'desc' } }),

    getAuditoriaActivas: async (): Promise<auditoria[]> =>
        await prisma.auditoria.findMany({
            where: { activo: 1 },
            orderBy: { fecha_creacion: 'desc' },
        }),

    getAuditoriaById: async (id_auditoria: number): Promise<auditoria | null> =>
        await prisma.auditoria.findUnique({ where: { id_auditoria } }),

    getAuditoriaBySensor: async (id_sensor: number): Promise<auditoria[]> =>
        await prisma.auditoria.findMany({
            where: { id_sensor },
            orderBy: { fecha_creacion: 'desc' },
        }),

    getAuditoriaByUsuario: async (usuario: string): Promise<auditoria[]> =>
        await prisma.auditoria.findMany({
            where: { usuario },
            orderBy: { fecha_creacion: 'desc' },
        }),

    getAuditoriaByAccion: async (accion: string): Promise<auditoria[]> =>
        await prisma.auditoria.findMany({
            where: { accion },
            orderBy: { fecha_creacion: 'desc' },
        }),

    getAuditoriaByRangoFechas: async (desde: string, hasta: string): Promise<auditoria[]> =>
        await prisma.auditoria.findMany({
            where: { fecha_creacion: { gte: desde, lte: hasta } },
            orderBy: { fecha_creacion: 'desc' },
        }),

    getUltimaAuditoriaBySensor: async (id_sensor: number): Promise<auditoria | null> =>
        await prisma.auditoria.findFirst({
            where: { id_sensor, activo: 1 },
            orderBy: { fecha_creacion: 'desc' },
        }),

    createAuditoria: async (data: AuditoriaCreate): Promise<auditoria> =>
        await prisma.auditoria.create({ data }),

    setActivoById: async (id_auditoria: number, activo: number): Promise<auditoria> =>
        await prisma.auditoria.update({ where: { id_auditoria }, data: { activo } }),

    deleteAuditoriaById: async (id_auditoria: number): Promise<auditoria> =>
        await prisma.auditoria.delete({ where: { id_auditoria } }),

    countBySensor: async (id_sensor: number): Promise<number> =>
        await prisma.auditoria.count({ where: { id_sensor } }),
};