// Archivo: ./src/backend/communication/layers/CapaFisica.ts
export interface MedioFisico {
  readonly esMedioNativo?: boolean;
  escribir(datos: Uint8Array, comoDatagrama?: boolean): void;
  alRecibir(callback: (datos: Uint8Array, esDatagrama?: boolean) => void): void;
  estaActivo(): boolean;
}

export class CapaFisica {
  private medios: Map<number, MedioFisico> = new Map();
  private oyentesSubida: Array<(idMedio: number, bytes: Uint8Array, esDatagrama: boolean) => void> = [];

  public vincularMedio(idMedio: number, medio: MedioFisico): void {
    this.medios.set(idMedio, medio);
    medio.alRecibir((bytes: Uint8Array, esDatagrama = false) => {
      if (bytes.length === 0) return;
      for (const oyente of this.oyentesSubida) {
        oyente(idMedio, bytes, esDatagrama);
      }
    });
  }

  public esMedioNativo(idMedio: number): boolean {
    return Boolean(this.medios.get(idMedio)?.esMedioNativo);
  }

  public obtenerTotalMedios(): number {
    return this.medios.size;
  }

  public estaActivo(idMedio: number): boolean {
    const medio = this.medios.get(idMedio);
    return Boolean(medio && medio.estaActivo());
  }

  public alSubirBytes(
    callback: (idMedio: number, bytes: Uint8Array, esDatagrama?: boolean) => void
  ): void {
    this.oyentesSubida.push(callback);
  }

  public enviarBytes(idMedio: number, datos: Uint8Array, comoDatagrama = false): number {
    const medio = this.medios.get(idMedio);
    if (!medio || !medio.estaActivo() || datos.length === 0) return 0;
    medio.escribir(datos, comoDatagrama);
    return datos.length;
  }
}