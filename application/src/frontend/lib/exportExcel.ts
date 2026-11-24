import type ExcelJSType from "exceljs";
import ExcelJSBrowser from "exceljs/dist/exceljs.min.js";
import { formatearFechaLocalCsv } from "./exportCsv";
import { calcularEstadisticasSerie } from "./estadisticasTermicas";
import type { LecturaSensorCliente } from "#types/rpc.types";

function obtenerExcelJS(): typeof ExcelJSType {
  const mod = ExcelJSBrowser as unknown as {
    Workbook?: typeof ExcelJSType.Workbook;
    default?: typeof ExcelJSType;
  };
  if (mod?.Workbook) return mod as unknown as typeof ExcelJSType;
  if (mod?.default?.Workbook) return mod.default;
  const globalLib = (window as unknown as { ExcelJS?: typeof ExcelJSType })
    .ExcelJS;
  if (globalLib?.Workbook) return globalLib;
  throw new Error("No se pudo cargar ExcelJS. Verificá 'npm install exceljs'.");
}

function descargarWorkbookBuffer(buffer: ArrayBuffer, nombreArchivo: string) {
  const nombreFinal = nombreArchivo.toLowerCase().endsWith(".xlsx")
    ? nombreArchivo
    : `${nombreArchivo}.xlsx`;
  const blob = new Blob([buffer], {
    type: "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
  });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = nombreFinal;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  window.setTimeout(() => URL.revokeObjectURL(url), 1500);
}

/**
 * Captura el <svg> principal de Recharts dentro de un contenedor a PNG Base64 (2x HiDPI)
 * resolviendo variables CSS para que se vea nítido sobre fondo blanco en Excel.
 */
export async function capturarGraficoComoPngBase64(
  contenedor: HTMLElement | null
): Promise<string | null> {
  if (!contenedor) return null;
  const svg =
    (contenedor.querySelector(
      ".recharts-wrapper svg.recharts-surface"
    ) as SVGSVGElement | null) ||
    (contenedor.querySelector("svg.recharts-surface") as SVGSVGElement | null);
  if (!svg) return null;

  const rect = svg.getBoundingClientRect();
  const width = Math.max(400, Math.round(rect.width || 720));
  const height = Math.max(220, Math.round(rect.height || 340));

  const clone = svg.cloneNode(true) as SVGSVGElement;
  clone.setAttribute("xmlns", "http://www.w3.org/2000/svg");
  clone.setAttribute("width", String(width));
  clone.setAttribute("height", String(height));
  clone.setAttribute("viewBox", `0 0 ${width} ${height}`);

  const bgRect = document.createElementNS("http://www.w3.org/2000/svg", "rect");
  bgRect.setAttribute("width", "100%");
  bgRect.setAttribute("height", "100%");
  bgRect.setAttribute("fill", "#ffffff");
  clone.insertBefore(bgRect, clone.firstChild);

  let xml = new XMLSerializer().serializeToString(clone);
  xml = xml
    .replace(/var\(--foreground-secondary\)/g, "#4b5563")
    .replace(/var\(--foreground\)/g, "#111827")
    .replace(/var\(--primary\)/g, "#059669")
    .replace(/var\(--secondary\)/g, "#f3f4f6")
    .replace(/var\(--background\)/g, "#ffffff")
    .replace(/currentColor/g, "#9ca3af");

  const svgBlob = new Blob([xml], { type: "image/svg+xml;charset=utf-8" });
  const svgUrl = URL.createObjectURL(svgBlob);

  try {
    const img = await new Promise<HTMLImageElement>((resolve, reject) => {
      const el = new Image();
      el.onload = () => resolve(el);
      el.onerror = reject;
      el.src = svgUrl;
    });

    const scale = 2;
    const canvas = document.createElement("canvas");
    canvas.width = width * scale;
    canvas.height = height * scale;
    const ctx = canvas.getContext("2d");
    if (!ctx) return null;

    ctx.fillStyle = "#ffffff";
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.scale(scale, scale);
    ctx.drawImage(img, 0, 0, width, height);
    return canvas.toDataURL("image/png");
  } catch {
    return null;
  } finally {
    URL.revokeObjectURL(svgUrl);
  }
}

export interface ColumnaExcel {
  header: string;
  width?: number;
  numFmt?: string;
}

export interface ResumenKpiExcel {
  etiqueta: string;
  valor: string | number;
  numFmt?: string;
}

export interface OpcionesTablaExcel {
  nombreArchivo: string;
  nombreHoja: string;
  titulo: string;
  subtitulo?: string;
  resumen?: ResumenKpiExcel[];
  columnas: ColumnaExcel[];
  filas: (string | number | boolean | null | undefined)[][];
}

export async function exportarTablaExcel(
  opciones: OpcionesTablaExcel
): Promise<void> {
  const ExcelJS = obtenerExcelJS();
  const wb = new ExcelJS.Workbook();
  wb.creator = "Sistema Monitoreo Termico UNTDF";
  wb.created = new Date();

  const hojaNombre = opciones.nombreHoja
    .replace(/[\\/?*[\]:]/g, "")
    .slice(0, 31);
  const ws = wb.addWorksheet(hojaNombre || "Datos");

  const totalCols = Math.max(2, opciones.columnas.length);

  // Banner título
  ws.mergeCells(1, 1, 1, totalCols);
  const cTitulo = ws.getCell(1, 1);
  cTitulo.value = opciones.titulo;
  cTitulo.font = { bold: true, size: 13, color: { argb: "FFFFFFFF" } };
  cTitulo.fill = {
    type: "pattern",
    pattern: "solid",
    fgColor: { argb: "FF0F766E" },
  };
  cTitulo.alignment = { vertical: "middle", horizontal: "left", indent: 1 };
  ws.getRow(1).height = 26;

  // Subtítulo + fecha generación
  ws.mergeCells(2, 1, 2, totalCols);
  const cSub = ws.getCell(2, 1);
  const ahoraTxt = new Date().toLocaleString("es-AR");
  cSub.value = opciones.subtitulo
    ? `${opciones.subtitulo}  |  Generado: ${ahoraTxt}`
    : `Generado: ${ahoraTxt}`;
  cSub.font = { italic: true, size: 10, color: { argb: "FF4B5563" } };
  cSub.fill = {
    type: "pattern",
    pattern: "solid",
    fgColor: { argb: "FFF3F4F6" },
  };
  cSub.alignment = { vertical: "middle", horizontal: "left", indent: 1 };
  ws.getRow(2).height = 20;

  let filaActual = 4;

  // Bloque resumen KPI opcional
  if (opciones.resumen && opciones.resumen.length > 0) {
    for (const kpi of opciones.resumen) {
      const r = ws.getRow(filaActual);
      r.getCell(1).value = kpi.etiqueta;
      r.getCell(1).font = { bold: true, size: 10, color: { argb: "FF374151" } };
      r.getCell(1).fill = {
        type: "pattern",
        pattern: "solid",
        fgColor: { argb: "FFF9FAFB" },
      };
      r.getCell(2).value = kpi.valor;
      r.getCell(2).font = { bold: true, size: 10, color: { argb: "FF0F766E" } };
      if (kpi.numFmt) r.getCell(2).numFmt = kpi.numFmt;
      filaActual++;
    }
    filaActual += 1;
  }

  // Cabecera de tabla
  const filaHeaderIdx = filaActual;
  const headerRow = ws.getRow(filaHeaderIdx);
  headerRow.height = 22;
  opciones.columnas.forEach((col, i) => {
    const cell = headerRow.getCell(i + 1);
    cell.value = col.header;
    cell.font = { bold: true, size: 10, color: { argb: "FFFFFFFF" } };
    cell.fill = {
      type: "pattern",
      pattern: "solid",
      fgColor: { argb: "FF1F2937" },
    };
    cell.alignment = { vertical: "middle", horizontal: "center" };
    cell.border = {
      bottom: { style: "medium", color: { argb: "FF111827" } },
    };
  });
  filaActual++;

  // Filas de datos
  const anchos = opciones.columnas.map((c) =>
    Math.max(c.width ?? 12, c.header.length + 4)
  );

  opciones.filas.forEach((fila, idxFila) => {
    const row = ws.getRow(filaActual++);
    const esPar = idxFila % 2 === 1;
    fila.forEach((val, colIdx) => {
      const cell = row.getCell(colIdx + 1);
      cell.value = val ?? "";
      cell.font = { size: 10 };
      if (esPar) {
        cell.fill = {
          type: "pattern",
          pattern: "solid",
          fgColor: { argb: "FFF9FAFB" },
        };
      }
      cell.border = {
        bottom: { style: "hair", color: { argb: "FFE5E7EB" } },
      };
      const colSpec = opciones.columnas[colIdx];
      if (typeof val === "number") {
        cell.alignment = { horizontal: "right" };
        if (colSpec?.numFmt) cell.numFmt = colSpec.numFmt;
      } else {
        cell.alignment = { horizontal: "left" };
      }
      const largo = String(val ?? "").length + 3;
      if (largo > anchos[colIdx]) anchos[colIdx] = Math.min(46, largo);
    });
  });

  anchos.forEach((w, i) => {
    ws.getColumn(i + 1).width = w;
  });

  ws.autoFilter = {
    from: { row: filaHeaderIdx, column: 1 },
    to: { row: Math.max(filaHeaderIdx, filaActual - 1), column: totalCols },
  };
  ws.views = [{ state: "frozen", ySplit: filaHeaderIdx }];

  const buf = await wb.xlsx.writeBuffer();
  descargarWorkbookBuffer(buf as ArrayBuffer, opciones.nombreArchivo);
}

export interface OpcionesCurvaExcel {
  nombreArchivo: string;
  nombreSensor: string;
  mac: string;
  ubicacion: string;
  subtitulo: string;
  umbralInf?: number;
  umbralSup?: number;
  serie: LecturaSensorCliente[];
  graficoContenedor?: HTMLElement | null;
}

export async function exportarCurvaTermicaExcel(
  opciones: OpcionesCurvaExcel
): Promise<void> {
  const ExcelJS = obtenerExcelJS();
  const wb = new ExcelJS.Workbook();
  wb.creator = "Sistema Monitoreo Termico UNTDF";
  wb.created = new Date();

  const stats = calcularEstadisticasSerie(
    opciones.serie.map((s) => ({
      timestamp: s.timestampMs,
      valor: s.temperaturaCorregida,
    })),
    opciones.umbralInf,
    opciones.umbralSup
  );

  // =========================================================================
  // HOJA 1: ANÁLISIS ESTADÍSTICO (CON FÓRMULAS EXCEL) + GRÁFICO INCRUSTADO
  // =========================================================================
  const wsRes = wb.addWorksheet("Analisis y Grafico");
  wsRes.mergeCells("A1:K1");
  const tCell = wsRes.getCell("A1");
  tCell.value = `Informe de Análisis Térmico · ${opciones.nombreSensor || opciones.mac}`;
  tCell.font = { bold: true, size: 14, color: { argb: "FFFFFFFF" } };
  tCell.fill = {
    type: "pattern",
    pattern: "solid",
    fgColor: { argb: "FF0F766E" },
  };
  tCell.alignment = { vertical: "middle", horizontal: "left", indent: 1 };
  wsRes.getRow(1).height = 28;

  wsRes.mergeCells("A2:K2");
  const sCell = wsRes.getCell("A2");
  sCell.value = `${opciones.subtitulo}  |  MAC: ${opciones.mac}  |  Ubicación: ${opciones.ubicacion || "Sin asignar"}  |  Generado: ${new Date().toLocaleString("es-AR")}`;
  sCell.font = { italic: true, size: 10, color: { argb: "FF374151" } };
  sCell.fill = {
    type: "pattern",
    pattern: "solid",
    fgColor: { argb: "FFF3F4F6" },
  };
  sCell.alignment = { vertical: "middle", horizontal: "left", indent: 1 };
  wsRes.getRow(2).height = 20;

  // Parámetros del sensor (Celdas B5 y B6 se referencian desde las fórmulas)
  const metaRows: [string, string | number, string?][] = [
    ["Sensor", opciones.nombreSensor || "DS18B20"],
    ["Umbral Inferior TL (°C)", opciones.umbralInf ?? -55, "0.00"],
    ["Umbral Superior TH (°C)", opciones.umbralSup ?? 126, "0.00"],
  ];
  metaRows.forEach(([k, v, fmt], idx) => {
    const r = wsRes.getRow(4 + idx);
    r.getCell(1).value = k;
    r.getCell(1).font = { bold: true, size: 10 };
    r.getCell(2).value = v;
    r.getCell(2).font = { bold: true, size: 10, color: { argb: "FF0F766E" } };
    if (fmt) r.getCell(2).numFmt = fmt;
  });

  // Tabla de estadísticos con FÓRMULAS REALES sobre la hoja Datos
  const lastRow = opciones.serie.length + 1;
  const rangoT = `Datos!F2:F${lastRow}`;
  const rangoX = `Datos!E2:E${lastRow}`;

  const hdrEst = wsRes.getRow(8);
  hdrEst.height = 22;
  ["Indicador Estadístico", "Valor (Fórmula)", "Fórmula Matemática"].forEach(
    (h, idx) => {
      const c = hdrEst.getCell(idx + 1);
      c.value = h;
      c.font = { bold: true, size: 10, color: { argb: "FFFFFFFF" } };
      c.fill = {
        type: "pattern",
        pattern: "solid",
        fgColor: { argb: "FF1F2937" },
      };
      c.alignment = { vertical: "middle", horizontal: "center" };
    }
  );

  const tieneMuestras = opciones.serie.length > 0;
  const tieneAlMenosDos = opciones.serie.length >= 2;
  const indicadores: {
    nombre: string;
    formula?: string;
    resultado: number;
    numFmt: string;
    nota: string;
  }[] = [
    {
      nombre: "Cantidad de lecturas (n)",
      formula: tieneMuestras ? `COUNTA(${rangoT})` : undefined,
      resultado: stats?.n ?? 0,
      numFmt: "0",
      nota: "Total de muestras cargadas",
    },
    {
      nombre: "Media aritmética (x̄)",
      formula: tieneMuestras ? `AVERAGE(${rangoT})` : undefined,
      resultado: stats?.promedio ?? 0,
      numFmt: '0.00" °C"',
      nota: "x̄ = (1/n) Σ Tᵢ",
    },
    {
      nombre: "Desv. estándar muestral (s)",
      formula: tieneAlMenosDos ? `STDEV.S(${rangoT})` : undefined,
      resultado: stats?.desviacionStd ?? 0,
      numFmt: '0.00" °C"',
      nota: "s = √[Σ(Tᵢ − x̄)² / (n − 1)]",
    },
    {
      nombre: "Mediana (P50)",
      formula: tieneMuestras ? `MEDIAN(${rangoT})` : undefined,
      resultado: stats?.mediana ?? 0,
      numFmt: '0.00" °C"',
      nota: "Percentil 50 (robusto a picos)",
    },
    {
      nombre: "Temperatura Mínima",
      formula: tieneMuestras ? `MIN(${rangoT})` : undefined,
      resultado: stats?.min ?? 0,
      numFmt: '0.00" °C"',
      nota: "mín(Tᵢ)",
    },
    {
      nombre: "Temperatura Máxima",
      formula: tieneMuestras ? `MAX(${rangoT})` : undefined,
      resultado: stats?.max ?? 0,
      numFmt: '0.00" °C"',
      nota: "máx(Tᵢ)",
    },
    {
      nombre: "Amplitud térmica (Δ)",
      formula: tieneMuestras ? `MAX(${rangoT})-MIN(${rangoT})` : undefined,
      resultado: stats?.rango ?? 0,
      numFmt: '0.00" °C"',
      nota: "T_max − T_min",
    },
    {
      nombre: "Percentil 5% (P05)",
      formula: tieneMuestras ? `PERCENTILE.INC(${rangoT},0.05)` : undefined,
      resultado: stats?.p05 ?? 0,
      numFmt: '0.00" °C"',
      nota: "Límite inferior 90% central",
    },
    {
      nombre: "Percentil 95% (P95)",
      formula: tieneMuestras ? `PERCENTILE.INC(${rangoT},0.95)` : undefined,
      resultado: stats?.p95 ?? 0,
      numFmt: '0.00" °C"',
      nota: "Límite superior 90% central",
    },
    {
      nombre: "Pendiente MCO (β₁)",
      formula: tieneAlMenosDos ? `SLOPE(${rangoT},${rangoX})` : undefined,
      resultado: stats?.pendienteCporHora ?? 0,
      numFmt: '+0.00" °C/h";-0.00" °C/h";0.00" °C/h"',
      nota: "Tasa de cambio lineal por hora",
    },
    {
      nombre: "Bondad de ajuste (R²)",
      formula: tieneAlMenosDos ? `RSQ(${rangoT},${rangoX})` : undefined,
      resultado: stats?.rCuadrado ?? 0,
      numFmt: "0.000",
      nota: "1 − SSE / SST",
    },
    {
      nombre: "Cumplimiento [TL, TH]",
      formula: tieneMuestras
        ? `COUNTIFS(${rangoT},">="&B5,${rangoT},"<="&B6)/COUNTA(${rangoT})`
        : undefined,
      resultado: (stats?.cumplimientoPct ?? 100) / 100,
      numFmt: "0.0%",
      nota: "Fracción dentro del rango seguro",
    },
  ];

  indicadores.forEach((ind, idx) => {
    const r = wsRes.getRow(9 + idx);
    r.getCell(1).value = ind.nombre;
    r.getCell(1).font = { bold: true, size: 10 };
    r.getCell(2).value = ind.formula
      ? { formula: ind.formula, result: ind.resultado }
      : ind.resultado;
    r.getCell(2).numFmt = ind.numFmt;
    r.getCell(2).font = { bold: true, size: 10, color: { argb: "FF0F766E" } };
    r.getCell(2).alignment = { horizontal: "right" };
    r.getCell(3).value = ind.nota;
    r.getCell(3).font = { size: 9, color: { argb: "FF6B7280" } };
    for (let c = 1; c <= 3; c++) {
      r.getCell(c).border = {
        bottom: { style: "hair", color: { argb: "FFE5E7EB" } },
      };
    }
  });

  wsRes.getColumn(1).width = 28;
  wsRes.getColumn(2).width = 18;
  wsRes.getColumn(3).width = 28;

  // Incrustar imagen del gráfico renderizado
  const pngDataUrl = await capturarGraficoComoPngBase64(
    opciones.graficoContenedor ?? null
  );
  if (pngDataUrl) {
    const imgId = wb.addImage({
      base64: pngDataUrl,
      extension: "png",
    });
    wsRes.addImage(imgId, {
      tl: { col: 3.4, row: 3.2 },
      ext: { width: 680, height: 350 },
    });
  }

  // =========================================================================
  // HOJA 2: DATOS CRUDOS Y TIEMPO TRANSCURRIDO (REFERENCIADA POR FÓRMULAS)
  // =========================================================================
  const wsDat = wb.addWorksheet("Datos");
  const colsDatos: ColumnaExcel[] = [
    { header: "N°", width: 8, numFmt: "0" },
    { header: "ID Sensor", width: 11, numFmt: "0" },
    { header: "MAC", width: 20 },
    { header: "Fecha / Hora Local", width: 22 },
    { header: "Horas desde t0 (h)", width: 18, numFmt: "0.0000" },
    { header: "Temp. Corregida (°C)", width: 20, numFmt: "0.00" },
    { header: "Umbral TL (°C)", width: 15, numFmt: "0.00" },
    { header: "Umbral TH (°C)", width: 15, numFmt: "0.00" },
    { header: "Estado Rango", width: 14 },
    { header: "Timestamp Unix", width: 16, numFmt: "0" },
  ];

  const hRow = wsDat.getRow(1);
  hRow.height = 22;
  colsDatos.forEach((col, i) => {
    const cell = hRow.getCell(i + 1);
    cell.value = col.header;
    cell.font = { bold: true, size: 10, color: { argb: "FFFFFFFF" } };
    cell.fill = {
      type: "pattern",
      pattern: "solid",
      fgColor: { argb: "FF1F2937" },
    };
    cell.alignment = { vertical: "middle", horizontal: "center" };
    wsDat.getColumn(i + 1).width = col.width ?? 14;
  });

  const t0Seg =
    opciones.serie.length > 0
      ? opciones.serie[0].timestampMs >= 10_000_000_000
        ? opciones.serie[0].timestampMs / 1000
        : opciones.serie[0].timestampMs
      : 0;
  const tlVal = opciones.umbralInf ?? -55;
  const thVal = opciones.umbralSup ?? 126;

  opciones.serie.forEach((item, idx) => {
    const rIdx = idx + 2;
    const tSeg =
      item.timestampMs >= 10_000_000_000
        ? item.timestampMs / 1000
        : item.timestampMs;
    const horasDesdeT0 =
      tSeg > 0 && t0Seg > 0 ? Number(((tSeg - t0Seg) / 3600).toFixed(5)) : idx / 60;
    const temp = Number(item.temperaturaCorregida.toFixed(2));
    const zona = temp > thVal ? "ALTA" : temp < tlVal ? "BAJA" : "OK";

    const row = wsDat.getRow(rIdx);
    row.getCell(1).value = idx + 1;
    row.getCell(2).value = item.idSensor;
    row.getCell(3).value = item.macHex;
    row.getCell(4).value = formatearFechaLocalCsv(item.timestampMs);
    row.getCell(5).value = horasDesdeT0;
    row.getCell(6).value = temp;
    row.getCell(7).value = tlVal;
    row.getCell(8).value = thVal;
    row.getCell(9).value = {
      formula: `IF(F${rIdx}>H${rIdx},"ALTA",IF(F${rIdx}<G${rIdx},"BAJA","OK"))`,
      result: zona,
    };
    row.getCell(10).value = item.timestampMs;

    colsDatos.forEach((c, ci) => {
      if (c.numFmt) row.getCell(ci + 1).numFmt = c.numFmt;
    });
  });

  wsDat.autoFilter = {
    from: { row: 1, column: 1 },
    to: { row: Math.max(1, lastRow), column: colsDatos.length },
  };
  wsDat.views = [{ state: "frozen", ySplit: 1 }];

  const buf = await wb.xlsx.writeBuffer();
  descargarWorkbookBuffer(buf as ArrayBuffer, opciones.nombreArchivo);
}