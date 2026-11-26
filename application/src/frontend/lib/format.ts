export function formatearEpochSegundos(epochSeg: number): string {
  if (!epochSeg || epochSeg <= 0) return "Sin registro";
  const ms = epochSeg < 10_000_000_000 ? epochSeg * 1000 : epochSeg;
  return new Date(ms).toLocaleString("es-AR", {
    day: "2-digit",
    month: "2-digit",
    year: "numeric",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

export function fechaAInputLocal(date: Date): string {
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(
    date.getDate()
  )}T${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

export function inputLocalAEpochSegundos(
  value: string,
  finDeMinuto = false
): number {
  if (!value) return 0;
  const ms = new Date(value).getTime();
  if (!Number.isFinite(ms)) return 0;
  const seg = Math.floor(ms / 1000);
  // datetime-local sin segundos trunca a :00; para extremos "Hasta" incluimos todo ese minuto (:59)
  return finDeMinuto && value.length <= 16 ? seg + 59 : seg;
}

export function redondearDecimal(valor: number, decimales = 2): number {
  if (!Number.isFinite(valor)) return 0;
  const factor = 10 ** decimales;
  const r = Math.round((valor + Number.EPSILON) * factor) / factor;
  return Object.is(r, -0) ? 0 : r;
}

export function formatearNumeroInput(valor: number, decimales = 2): string {
  if (!Number.isFinite(valor)) return "";
  const limpio = redondearDecimal(valor, decimales);
  return String(limpio);
}

export function parsearNumeroInput(
  raw: string,
  decimales = 2
): number | null {
  const normalizado = raw.trim().replace(",", ".");
  if (
    normalizado === "" ||
    normalizado === "-" ||
    normalizado === "." ||
    normalizado === "-."
  ) {
    return null;
  }
  const n = Number(normalizado);
  if (!Number.isFinite(n)) return null;
  return redondearDecimal(n, decimales);
}

export function inputLocalAFechaIsoSql(value: string): string {
  if (!value) return "";
  const d = new Date(value);
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ${pad(
    d.getHours()
  )}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
}

export function formatearDuracionSegundos(segundos: number): string {
  if (!segundos || segundos < 0) return "0s";
  const d = Math.floor(segundos / 86400);
  const h = Math.floor((segundos % 86400) / 3600);
  const m = Math.floor((segundos % 3600) / 60);
  const s = segundos % 60;
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

export function formatearBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
}