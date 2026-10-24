// src/backend/stubs/servicioExample.ts

// ===== uso desde cualquier servicio del backend =====
import { firmware } from "../stubs/clientExample";

const r = await firmware.sumarYLeerEstado(3, 4);
console.log(r.resultado, r.estado);