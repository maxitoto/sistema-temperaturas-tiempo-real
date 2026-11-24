import { redondearDecimal } from "./format";
import type { CalibracionAmbienteDTO } from "#types/rpc.types";

// ============================================================================
// 1. MODELO ATMOSFÉRICO BAROMÉTRICO (espejo de ModeloAtmosfericoBarometrico.hpp)
// ============================================================================
export const PRESION_NIVEL_MAR_HPA = 1013.25;
const TEMP_EBULLICION_STD_K = 373.15;
const CERO_CELSIUS_K = 273.15;
const CONSTANTE_R_GAS = 8.31446;
const ENTALPIA_VAPOR_AGUA = 40660.0;
const GRADIENTE_ISA_INV_M = 2.25577e-5;
const EXPONENTE_BAROMETRICO = 5.25588;

export function estimarPresionDesdeAltura(alturaMetros: number): number {
  const h = Math.max(-500, Math.min(9000, Number.isFinite(alturaMetros) ? alturaMetros : 0));
  const base = 1 - GRADIENTE_ISA_INV_M * h;
  const p = PRESION_NIVEL_MAR_HPA * Math.pow(base, EXPONENTE_BAROMETRICO);
  return redondearDecimal(Math.max(300, Math.min(1100, p)), 2);
}

export function estimarEbullicionDesdePresion(presionHpa: number): number {
  const p = Math.max(
    300,
    Math.min(1100, Number.isFinite(presionHpa) && presionHpa > 0 ? presionHpa : PRESION_NIVEL_MAR_HPA)
  );
  const invT =
    1 / TEMP_EBULLICION_STD_K -
    (CONSTANTE_R_GAS / ENTALPIA_VAPOR_AGUA) * Math.log(p / PRESION_NIVEL_MAR_HPA);
  return redondearDecimal(1 / invT - CERO_CELSIUS_K, 2);
}

// ============================================================================
// 2. MODELO CALIBRACIÓN 2 PUNTOS (espejo de ModeloCalibracionDosPuntos.hpp)
// ============================================================================
export interface ParametrosRectaCalibracion {
  pendienteM: number;
  ordenadaB: number;
  usaDosPuntos: boolean;
}

export function obtenerReferenciasActivas(
  ambiente: CalibracionAmbienteDTO | null
): { refCong: number; refEbull: number } {
  return {
    refCong: ambiente?.activa ? ambiente.tempCongelacionRef : 0,
    refEbull: ambiente?.activa ? ambiente.tempEbullicionRef : 100,
  };
}

export function calcularRectaCalibracion(
  medicionHieloC: number,
  medicionEbullicionC: number,
  refCongelacionC: number,
  refEbullicionC: number,
  offsetFallbackC = 0
): ParametrosRectaCalibracion {
  const deltaMedido = medicionEbullicionC - medicionHieloC;
  const deltaRef = refEbullicionC - refCongelacionC;

  if (
    Number.isFinite(deltaMedido) &&
    Number.isFinite(deltaRef) &&
    Math.abs(deltaMedido) > 1.0 &&
    Math.abs(deltaRef) > 1.0
  ) {
    const pendienteM = deltaRef / deltaMedido;
    const ordenadaB = refCongelacionC - pendienteM * medicionHieloC;
    return { pendienteM, ordenadaB, usaDosPuntos: true };
  }

  return {
    pendienteM: 1,
    ordenadaB: Number.isFinite(offsetFallbackC) ? offsetFallbackC : 0,
    usaDosPuntos: false,
  };
}

export function reconstruirTempCrudaDesdeCorregida(
  tempCorregidaC: number,
  medicionHieloC: number,
  medicionEbullicionC: number,
  offsetFallbackC: number,
  ambiente: CalibracionAmbienteDTO | null
): number {
  const { refCong, refEbull } = obtenerReferenciasActivas(ambiente);
  const p = calcularRectaCalibracion(
    medicionHieloC,
    medicionEbullicionC,
    refCong,
    refEbull,
    offsetFallbackC
  );
  if (Math.abs(p.pendienteM) < 0.001) return redondearDecimal(tempCorregidaC, 2);
  return redondearDecimal((tempCorregidaC - p.ordenadaB) / p.pendienteM, 2);
}

export function formatearEcuacionRecta(p: ParametrosRectaCalibracion): string {
  const signo = p.ordenadaB >= 0 ? "+" : "−";
  const absB = Math.abs(p.ordenadaB).toFixed(2);
  if (!p.usaDosPuntos) {
    if (Math.abs(p.ordenadaB) < 0.005) return "T_cal = T_cruda (sin calibrar)";
    return `T_cal = T_cruda ${signo} ${absB} °C`;
  }
  return `T_cal = ${p.pendienteM.toFixed(4)} · T_cruda ${signo} ${absB} °C`;
}