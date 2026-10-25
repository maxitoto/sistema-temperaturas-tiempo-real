// Archivo: ./src/backend/database/prismaClient.ts
import { createRequire } from "node:module";
import * as fs from "node:fs";
import * as path from "node:path";
import type { PrismaClient as PrismaClientType } from "@prisma/client";

// Prisma se carga vía CommonJS porque el binding nativo (query_engine-*.node)
// falla al inicializarse directamente desde ESM dentro de Electron.
const require = createRequire(import.meta.url);
const { PrismaClient } = require("@prisma/client") as {
  PrismaClient: new (options?: {
    datasources?: { db?: { url?: string } };
  }) => PrismaClientType;
};

const rootDir = process.env.APP_ROOT || process.cwd();
const dbDir = path.resolve(rootDir, "src", "backend", "database");

// SQLite no crea directorios: sin este mkdir, la primera query falla con SQLITE_CANTOPEN.
// `recursive: true` lo hace idempotente (no falla si ya existe).
if (!fs.existsSync(dbDir)) {
  fs.mkdirSync(dbDir, { recursive: true });
}

const dbFilePath = path.join(dbDir, "monitor.db");

/**
 * Instancia única de PrismaClient para todo el backend.
 *
 * Se pasa `datasources.db.url` en el constructor para sobreescribir la URL
 * relativa de `schema.prisma` con una ruta absoluta resuelta en runtime.
 * Sin esto, el mismo código falla en otro OS o en producción empaquetada.
 *
 * @example
 * import prisma from './prismaClient';
 * const sensores = await prisma.sensor.findMany();
 *
 * @throws {Error} Si `APP_ROOT` no es escribible y no se puede crear `dbDir`.
 */
const prisma = new PrismaClient({
  datasources: {
    db: {
      url: `file:${dbFilePath}`,
    },
  },
});

export default prisma;