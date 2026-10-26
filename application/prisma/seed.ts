// ./prisma/seed.ts
import { PrismaClient } from "@prisma/client";

const prisma = new PrismaClient();

async function main() {
  /* 
  aqui se puede inicializar datos la base de datos 
  ejemplo:
    await prisma.conexion_firmware.upsert({
      where: { id_conexion: 1 },
      update: {},
      create: {
        id_conexion: 1,
        ip_dhcp: "127.0.0.1",
        puerto_udp: 81,
        puerto_usb: "/tmp/virtual_serial",
        baudios_usb: 115200,
      },
    });
  */
}

main()
  .then(() => prisma.$disconnect())
  .catch(async (e) => {
    console.error(e);
    await prisma.$disconnect();
    process.exit(1);
  });