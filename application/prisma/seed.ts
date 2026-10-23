import { PrismaClient } from "@prisma/client";

const prisma = new PrismaClient();

async function main() {
  await prisma.ubicacion.upsert({
    where: { nombre: "Sin Asignar" },
    update: {},
    create: {
      nombre: "Sin Asignar",
      descripcion: "Ubicación inicial por defecto para sensores autodescubiertos en el bus OneWire",
    },
  });
}

main()
  .then(() => prisma.$disconnect())
  .catch(async (e) => {
    console.error(e);
    await prisma.$disconnect();
    process.exit(1);
  });