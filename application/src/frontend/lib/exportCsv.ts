import { formatearEpochSegundos } from "./format";

function escaparCeldaCsv(valor: unknown): string {
  const texto = valor === null || valor === undefined ? "" : String(valor);
  if (/[",\r\n]/.test(texto)) {
    return `"${texto.replace(/"/g, '""')}"`;
  }
  return texto;
}

export function sufijoFechaArchivo(fecha = new Date()): string {
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${fecha.getFullYear()}${pad(fecha.getMonth() + 1)}${pad(
    fecha.getDate()
  )}-${pad(fecha.getHours())}${pad(fecha.getMinutes())}`;
}

export function formatearIsoUtcDesdeEpoch(epochSeg: number): string {
  if (!epochSeg || epochSeg <= 0) return "";
  const ms = epochSeg < 10_000_000_000 ? epochSeg * 1000 : epochSeg;
  return new Date(ms).toISOString();
}

export function descargarCsv(
  nombreArchivo: string,
  encabezados: string[],
  filas: Array<Array<string | number | boolean | null | undefined>>
): void {
  const lineas: string[] = [
    encabezados.map(escaparCeldaCsv).join(","),
    ...filas.map((fila) => fila.map(escaparCeldaCsv).join(",")),
  ];

  // BOM UTF-8 (\uFEFF) para que Excel/LibreOffice muestre tildes y '°C' correctamente
  const contenido = `\uFEFF${lineas.join("\r\n")}\r\n`;
  const blob = new Blob([contenido], { type: "text/csv;charset=utf-8;" });
  const url = URL.createObjectURL(blob);

  const enlace = document.createElement("a");
  enlace.href = url;
  enlace.download = nombreArchivo.endsWith(".csv")
    ? nombreArchivo
    : `${nombreArchivo}.csv`;
  document.body.appendChild(enlace);
  enlace.click();
  document.body.removeChild(enlace);
  window.setTimeout(() => URL.revokeObjectURL(url), 1000);
}

export function formatearFechaLocalCsv(epochSeg: number): string {
  return formatearEpochSegundos(epochSeg);
}