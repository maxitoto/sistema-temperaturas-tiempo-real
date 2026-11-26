function normalizarEpochMs(epoch: number): number {
  if (!Number.isFinite(epoch) || epoch <= 0) return 0;
  return epoch >= 10_000_000_000 ? epoch : epoch * 1000;
}

function escaparCeldaCsv(valor: unknown): string {
  const texto = valor === null || valor === undefined ? "" : String(valor);
  if (/[";\r\n]/.test(texto)) {
    return `"${texto.replace(/"/g, '""')}"`;
  }
  return texto;
}

export function formatearIsoUtcDesdeEpoch(epoch: number): string {
  const ms = normalizarEpochMs(epoch);
  if (ms <= 0) return "";
  return new Date(ms).toISOString();
}

export function formatearFechaLocalCsv(epoch: number): string {
  const ms = normalizarEpochMs(epoch);
  if (ms <= 0) return "";
  const d = new Date(ms);
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
}

export function sufijoFechaArchivo(fecha = new Date()): string {
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${fecha.getFullYear()}${pad(fecha.getMonth() + 1)}${pad(fecha.getDate())}_${pad(fecha.getHours())}${pad(fecha.getMinutes())}`;
}

export function descargarCsv(
  nombreArchivo: string,
  encabezados: string[],
  filas: (string | number | boolean | null | undefined)[][]
): void {
  const lineas: string[] = [
    encabezados.map(escaparCeldaCsv).join(";"),
    ...filas.map((fila) => fila.map(escaparCeldaCsv).join(";")),
  ];
  const contenido = "\uFEFF" + lineas.join("\r\n");
  const blob = new Blob([contenido], { type: "text/csv;charset=utf-8;" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = nombreArchivo.toLowerCase().endsWith(".csv")
    ? nombreArchivo
    : `${nombreArchivo}.csv`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  window.setTimeout(() => URL.revokeObjectURL(url), 1500);
}