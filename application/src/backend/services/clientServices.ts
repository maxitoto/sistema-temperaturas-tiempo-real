// Archivo: ./src/backend/services/clientServices.ts
import { firmware } from "../stubs/clientExample";
import { serviciosConectivos } from "../communication/conectividad/servicosConectivos";
import { consultasMixtas } from '../database/consultas/consultasMixtas';
import { auditoriaRepository } from '../database/consultas/repositories/auditoriaRepository';
import { sensorRepository } from '../database/consultas/repositories/sensorRepository';
import { conexionFirmwareRepository } from '../database/consultas/repositories/conexionFirmwareRepository';

export const clientServices = {
  ...serviciosConectivos,
  ...auditoriaRepository,
  ...consultasMixtas,
  ...sensorRepository,
  ...conexionFirmwareRepository,
  ...firmware,
};