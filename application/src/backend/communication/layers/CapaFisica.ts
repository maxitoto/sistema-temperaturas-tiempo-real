// Archivo: ./src/communication/layers/CapaFisica.ts

export interface MedioFisico {
  escribir(datos: Uint8Array): void;
  alRecibir(callback: (datos: Uint8Array) => void): void;
  estaActivo(): boolean;
}

export class CapaFisica {
  private medios: Map<number, MedioFisico> = new Map();
  private oyentesSubida: Array<(idMedio: number, bytes: Uint8Array) => void> = [];

  public vincularMedio(idMedio: number, medio: MedioFisico): void {
    this.medios.set(idMedio, medio);
    medio.alRecibir((bytes: Uint8Array) => {
      if (bytes.length === 0) return;
      for (const oyente of this.oyentesSubida) {
        oyente(idMedio, bytes);
      }
    });
  }

  public obtenerTotalMedios(): number {
    return this.medios.size;
  }

  public estaActivo(idMedio: number): boolean {
    const medio = this.medios.get(idMedio);
    return Boolean(medio && medio.estaActivo());
  }

  // CAMINO DE SUBIDA (RX): Entrega ráfagas de bytes a la Capa de Enlace
  public alSubirBytes(callback: (idMedio: number, bytes: Uint8Array) => void): void {
    this.oyentesSubida.push(callback);
  }

  // CAMINO DE BAJADA (TX): Escribe bytes en el descriptor físico
  public enviarBytes(idMedio: number, datos: Uint8Array): number {
    const medio = this.medios.get(idMedio);
    if (!medio || !medio.estaActivo() || datos.length === 0) return 0;
    medio.escribir(datos);
    return datos.length;
  }
}