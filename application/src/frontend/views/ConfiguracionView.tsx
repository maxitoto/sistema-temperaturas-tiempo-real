import React, { useCallback, useEffect, useState } from "react";
import {
  Settings,
  Wifi,
  Mail,
  Clock,
  Save,
  Plus,
  Trash2,
  Edit2,
  RefreshCw,
} from "lucide-react";
import { useAuth } from "../contexts/AuthContext";
import { client, safeRpc } from "../lib/service";
import { formatearEpochSegundos } from "../lib/format";
import { LABEL_ESTADO_ENVIO, LABEL_TIPO_RECEPCION } from "../lib/labels";
import {
  ConfiguracionSistemaDTO,
  ConfiguracionSmtpDTO,
  ConfiguracionWiFiCliente,
  CorreoItemDTO,
  RolUsuario,
  ServidorNtpItemDTO,
  TipoRecepcionCorreo,
} from "#types/rpc.types";

export function ConfiguracionView() {
  const { rol } = useAuth();
  const esAdmin = rol === RolUsuario.ADMIN;

  const [tab, setTab] = useState<"SISTEMA" | "WIFI" | "CORREOS" | "NTP">(
    "SISTEMA"
  );

  // --- Pestaña 1: Sistema y Retención ---
  const [cfgSistema, setCfgSistema] = useState<ConfiguracionSistemaDTO | null>(
    null
  );
  const [formSistema, setFormSistema] = useState({
    zonaHorariaMin: -180,
    retencionSensoDias: 30,
    retencionEventosDias: 60,
    retencionAuditoriaDias: 90,
    sdLibreMinimoKb: 2048,
    correoCooldownS: 300,
    activa: true,
  });

  // --- Pestaña 2: WiFi STA + SoftAP (Solo ADMIN) ---
  const [wifiInfo, setWifiInfo] = useState<ConfiguracionWiFiCliente | null>(
    null
  );
  const [formWifi, setFormWifi] = useState({
    ssidSta: "",
    claveSta: "",
    ssidAp: "",
    claveAp: "",
  });

  // --- Pestaña 3: SMTP y Destinatarios de Correo ---
  const [correos, setCorreos] = useState<CorreoItemDTO[]>([]);
  const [formCorreo, setFormCorreo] = useState({
    idCorreo: 0,
    nombre: "",
    apellido: "",
    correoElectronico: "",
    tipoRecepcionMensajes: TipoRecepcionCorreo.RECIBIR_EVENTOS_CRITICOS,
    activa: true,
  });
  const [smtpCfg, setSmtpCfg] = useState<ConfiguracionSmtpDTO | null>(null);
  const [formSmtp, setFormSmtp] = useState({
    hostSmtp: "",
    puerto: 587,
    usarTls: true,
    usuario: "",
    passwordCifrado: "",
    emailRemitente: "",
    nombreRemitente: "ESP32 UNTDF",
    activa: true,
  });

  // --- Pestaña 4: Servidores NTP ---
  const [servidoresNtp, setServidoresNtp] = useState<ServidorNtpItemDTO[]>([]);
  const [formNtp, setFormNtp] = useState({
    idServer: 0,
    server: "",
    prioridad: 1,
    activa: true,
  });

  const cargarSistema = useCallback(async () => {
    const cfg = await safeRpc(
      () => client.obtenerConfiguracionSistema(),
      "obtenerConfiguracionSistema"
    );
    if (cfg) {
      setCfgSistema(cfg);
      setFormSistema({
        zonaHorariaMin: cfg.zonaHorariaMin,
        retencionSensoDias: cfg.retencionSensoDias,
        retencionEventosDias: cfg.retencionEventosDias,
        retencionAuditoriaDias: cfg.retencionAuditoriaDias,
        sdLibreMinimoKb: cfg.sdLibreMinimoKb,
        correoCooldownS: cfg.correoCooldownS,
        activa: cfg.activa,
      });
    }
  }, []);

  const cargarWifi = useCallback(async () => {
    if (!esAdmin) return;
    const w = await safeRpc(
      () => client.obtenerConfiguracionWiFi(),
      "obtenerConfiguracionWiFi"
    );
    if (w) {
      setWifiInfo(w);
      setFormWifi((prev) => ({
        ...prev,
        ssidSta: w.ssidSta,
        ssidAp: w.ssidAp,
      }));
    }
  }, [esAdmin]);

  const cargarCorreosYSmtp = useCallback(async () => {
    const lista = await safeRpc(() => client.listarCorreos(), "listarCorreos");
    if (lista) setCorreos(lista);

    if (esAdmin) {
      const smtp = await safeRpc(
        () => client.obtenerConfiguracionSmtp(),
        "obtenerConfiguracionSmtp",
        { silent: true }
      );
      if (smtp) {
        setSmtpCfg(smtp);
        setFormSmtp({
          hostSmtp: smtp.hostSmtp,
          puerto: smtp.puerto,
          usarTls: smtp.usarTls,
          usuario: smtp.usuario,
          passwordCifrado: smtp.passwordCifrado,
          emailRemitente: smtp.emailRemitente,
          nombreRemitente: smtp.nombreRemitente,
          activa: smtp.activa,
        });
      }
    }
  }, [esAdmin]);

  const cargarNtp = useCallback(async () => {
    const lista = await safeRpc(
      () => client.listarServidoresNtp(),
      "listarServidoresNtp"
    );
    if (lista) setServidoresNtp(lista);
  }, []);

  useEffect(() => {
    if (tab === "SISTEMA") void cargarSistema();
    if (tab === "WIFI") void cargarWifi();
    if (tab === "CORREOS") void cargarCorreosYSmtp();
    if (tab === "NTP") void cargarNtp();
  }, [tab, cargarSistema, cargarWifi, cargarCorreosYSmtp, cargarNtp]);

  // Handlers de Guardado (ADMIN)
  const guardarSistema = async (e: React.FormEvent) => {
    e.preventDefault();
    const res = await safeRpc(
      () =>
        client.actualizarConfiguracionSistema({
          zonaHorariaMin: Number(formSistema.zonaHorariaMin),
          retencionSensoDias: Number(formSistema.retencionSensoDias),
          retencionEventosDias: Number(formSistema.retencionEventosDias),
          retencionAuditoriaDias: Number(formSistema.retencionAuditoriaDias),
          sdLibreMinimoKb: Number(formSistema.sdLibreMinimoKb),
          correoCooldownS: Number(formSistema.correoCooldownS),
          activa: formSistema.activa,
        }),
      "actualizarConfiguracionSistema",
      { mensajeExito: "Configuración general del sistema guardada." }
    );
    if (res) setCfgSistema(res);
  };

  const guardarWifi = async (e: React.FormEvent) => {
    e.preventDefault();
    if (formWifi.claveSta.length > 0 && formWifi.claveSta.length < 8) {
      window.alert("La clave WiFi STA debe tener al menos 8 caracteres (WPA2).");
      return;
    }
    if (formWifi.claveAp.length < 8) {
      window.alert(
        "La clave del SoftAP debe tener al menos 8 caracteres (WPA2)."
      );
      return;
    }
    const res = await safeRpc(
      () =>
        client.actualizarConfiguracionWiFi(
          formWifi.ssidSta.trim(),
          formWifi.claveSta,
          formWifi.ssidAp.trim(),
          formWifi.claveAp
        ),
      "actualizarConfiguracionWiFi",
      { mensajeExito: "Parámetros WiFi STA/SoftAP actualizados." }
    );
    if (res) setWifiInfo(res);
  };

  const guardarSmtp = async (e: React.FormEvent) => {
    e.preventDefault();
    const res = await safeRpc(
      () =>
        client.actualizarConfiguracionSmtp({
          hostSmtp: formSmtp.hostSmtp.trim(),
          puerto: Number(formSmtp.puerto),
          usarTls: formSmtp.usarTls,
          usuario: formSmtp.usuario.trim(),
          passwordCifrado: formSmtp.passwordCifrado,
          emailRemitente: formSmtp.emailRemitente.trim(),
          nombreRemitente: formSmtp.nombreRemitente.trim(),
          activa: formSmtp.activa,
        }),
      "actualizarConfiguracionSmtp",
      { mensajeExito: "Configuración del servidor SMTP guardada." }
    );
    if (res) setSmtpCfg(res);
  };

  const guardarDestinatario = async (e: React.FormEvent) => {
    e.preventDefault();
    const res = await safeRpc(
      () =>
        client.guardarCorreo({
          idCorreo: formCorreo.idCorreo,
          nombre: formCorreo.nombre.trim(),
          apellido: formCorreo.apellido.trim(),
          correoElectronico: formCorreo.correoElectronico.trim(),
          tipoRecepcionMensajes: Number(
            formCorreo.tipoRecepcionMensajes
          ) as TipoRecepcionCorreo,
          activa: formCorreo.activa,
        }),
      "guardarCorreo",
      {
        mensajeExito:
          formCorreo.idCorreo === 0
            ? "Destinatario agregado."
            : "Destinatario actualizado.",
      }
    );
    if (res) {
      setFormCorreo({
        idCorreo: 0,
        nombre: "",
        apellido: "",
        correoElectronico: "",
        tipoRecepcionMensajes: TipoRecepcionCorreo.RECIBIR_EVENTOS_CRITICOS,
        activa: true,
      });
      void cargarCorreosYSmtp();
    }
  };

  const eliminarDestinatario = async (idCorreo: number) => {
    await safeRpc(
      () => client.eliminarCorreoPorId(idCorreo),
      "eliminarCorreoPorId",
      { mensajeExito: "Destinatario eliminado." }
    );
    void cargarCorreosYSmtp();
  };

  const guardarNtp = async (e: React.FormEvent) => {
    e.preventDefault();
    const res = await safeRpc(
      () =>
        client.guardarServidorNtp({
          idServer: formNtp.idServer,
          server: formNtp.server.trim(),
          prioridad: Number(formNtp.prioridad),
          activa: formNtp.activa,
        }),
      "guardarServidorNtp",
      {
        mensajeExito:
          formNtp.idServer === 0
            ? "Servidor NTP agregado."
            : "Servidor NTP actualizado.",
      }
    );
    if (res) {
      setFormNtp({ idServer: 0, server: "", prioridad: 1, activa: true });
      void cargarNtp();
    }
  };

  const eliminarNtp = async (idServer: number) => {
    await safeRpc(
      () => client.eliminarServidorNtpPorId(idServer),
      "eliminarServidorNtpPorId",
      { mensajeExito: "Servidor NTP eliminado." }
    );
    void cargarNtp();
  };

  return (
    <div className="space-y-4 p-4">
      {/* Encabezado y Pestañas */}
      <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
        <div className="flex items-center gap-3">
          <div className="flex h-10 w-10 items-center justify-center rounded-lg bg-primary-light text-primary">
            <Settings className="h-5 w-5" />
          </div>
          <div>
            <h1 className="text-lg font-bold">
              Configuración del Sistema, Red y Notificaciones
            </h1>
            <p className="text-xs text-foreground-secondary">
              {esAdmin
                ? "Modo Administrador: lectura y escritura habilitadas"
                : "Modo Operador: consulta en solo lectura"}
            </p>
          </div>
        </div>

        <div className="flex flex-wrap gap-1.5 rounded-lg bg-secondary p-1 text-xs">
          <button
            type="button"
            onClick={() => setTab("SISTEMA")}
            className={`flex items-center gap-1.5 rounded-md px-3 py-1.5 font-semibold ${
              tab === "SISTEMA"
                ? "bg-card text-foreground shadow-xs"
                : "text-foreground-secondary hover:text-foreground"
            }`}
          >
            <Settings className="h-3.5 w-3.5" />
            Sistema & Retención
          </button>
          {esAdmin && (
            <button
              type="button"
              onClick={() => setTab("WIFI")}
              className={`flex items-center gap-1.5 rounded-md px-3 py-1.5 font-semibold ${
                tab === "WIFI"
                  ? "bg-card text-foreground shadow-xs"
                  : "text-foreground-secondary hover:text-foreground"
              }`}
            >
              <Wifi className="h-3.5 w-3.5" />
              WiFi STA / SoftAP
            </button>
          )}
          <button
            type="button"
            onClick={() => setTab("CORREOS")}
            className={`flex items-center gap-1.5 rounded-md px-3 py-1.5 font-semibold ${
              tab === "CORREOS"
                ? "bg-card text-foreground shadow-xs"
                : "text-foreground-secondary hover:text-foreground"
            }`}
          >
            <Mail className="h-3.5 w-3.5" />
            SMTP & Correos
          </button>
          <button
            type="button"
            onClick={() => setTab("NTP")}
            className={`flex items-center gap-1.5 rounded-md px-3 py-1.5 font-semibold ${
              tab === "NTP"
                ? "bg-card text-foreground shadow-xs"
                : "text-foreground-secondary hover:text-foreground"
            }`}
          >
            <Clock className="h-3.5 w-3.5" />
            Servidores NTP
          </button>
        </div>
      </div>

      {/* ================================================================= */}
      {/* PESTAÑA 1: SISTEMA Y RETENCIÓN                                    */}
      {/* ================================================================= */}
      {tab === "SISTEMA" && (
        <form
          onSubmit={guardarSistema}
          className="space-y-4 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
        >
          <div className="flex items-center justify-between">
            <h2 className="text-sm font-bold">
              Políticas de Retención MicroSD, Zona Horaria y Cooldown
            </h2>
            {cfgSistema && (
              <span className="font-mono text-[11px] text-foreground-secondary">
                Última mod.:{" "}
                {formatearEpochSegundos(cfgSistema.fechaModificacion)}
              </span>
            )}
          </div>

          <div className="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-3 text-xs">
            <div>
              <label className="mb-1 block font-medium">
                Zona Horaria (minutos UTC, ej. -180 = UTC-3)
              </label>
              <input
                type="number"
                disabled={!esAdmin}
                value={formSistema.zonaHorariaMin}
                onChange={(e) =>
                  setFormSistema({
                    ...formSistema,
                    zonaHorariaMin: Number(e.target.value),
                  })
                }
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 block font-medium">
                Retención Tabla Senso (días)
              </label>
              <input
                type="number"
                min={1}
                disabled={!esAdmin}
                value={formSistema.retencionSensoDias}
                onChange={(e) =>
                  setFormSistema({
                    ...formSistema,
                    retencionSensoDias: Number(e.target.value),
                  })
                }
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 block font-medium">
                Retención Eventos Sensores (días)
              </label>
              <input
                type="number"
                min={1}
                disabled={!esAdmin}
                value={formSistema.retencionEventosDias}
                onChange={(e) =>
                  setFormSistema({
                    ...formSistema,
                    retencionEventosDias: Number(e.target.value),
                  })
                }
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 block font-medium">
                Retención Auditoría Auth/Sistema (días)
              </label>
              <input
                type="number"
                min={1}
                disabled={!esAdmin}
                value={formSistema.retencionAuditoriaDias}
                onChange={(e) =>
                  setFormSistema({
                    ...formSistema,
                    retencionAuditoriaDias: Number(e.target.value),
                  })
                }
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 block font-medium">
                Espacio Libre Mínimo MicroSD (KB)
              </label>
              <input
                type="number"
                min={128}
                disabled={!esAdmin}
                value={formSistema.sdLibreMinimoKb}
                onChange={(e) =>
                  setFormSistema({
                    ...formSistema,
                    sdLibreMinimoKb: Number(e.target.value),
                  })
                }
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 block font-medium">
                Cooldown Envío de Correos (segundos)
              </label>
              <input
                type="number"
                min={0}
                disabled={!esAdmin}
                value={formSistema.correoCooldownS}
                onChange={(e) =>
                  setFormSistema({
                    ...formSistema,
                    correoCooldownS: Number(e.target.value),
                  })
                }
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>
          </div>

          <label className="flex items-center gap-2 text-xs">
            <input
              type="checkbox"
              disabled={!esAdmin}
              checked={formSistema.activa}
              onChange={(e) =>
                setFormSistema({ ...formSistema, activa: e.target.checked })
              }
            />
            Purga automática y políticas de retención activas
          </label>

          {esAdmin && (
            <button
              type="submit"
              className="btn btn-solid btn-primary px-4 py-2 text-xs font-bold"
            >
              <Save className="h-4 w-4" />
              Guardar Configuración del Sistema
            </button>
          )}
        </form>
      )}

      {/* ================================================================= */}
      {/* PESTAÑA 2: WIFI STA + SOFTAP (Solo ADMIN)                         */}
      {/* ================================================================= */}
      {tab === "WIFI" && esAdmin && (
        <div className="space-y-4">
          {wifiInfo && (
            <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
              <div className="rounded-xl border border-border bg-card p-4 text-xs shadow-xs">
                <div className="font-semibold text-foreground-secondary">
                  Modo Estación (STA · Router)
                </div>
                <div className="mt-1 font-mono text-base font-bold">
                  {wifiInfo.ssidSta || "Sin configurar"} · IP{" "}
                  {wifiInfo.ipSta || "0.0.0.0"}
                </div>
                <div className="mt-1">
                  Estado:{" "}
                  <strong
                    className={
                      wifiInfo.conectadoARouter ? "text-success" : "text-warning"
                    }
                  >
                    {wifiInfo.conectadoARouter
                      ? "Conectado al Router"
                      : "Desconectado"}
                  </strong>
                </div>
              </div>

              <div className="rounded-xl border border-border bg-card p-4 text-xs shadow-xs">
                <div className="font-semibold text-foreground-secondary">
                  Punto de Acceso Local (SoftAP)
                </div>
                <div className="mt-1 font-mono text-base font-bold">
                  {wifiInfo.ssidAp || "ESP32-UNTDF"} · IP{" "}
                  {wifiInfo.ipAp || "192.168.4.1"}
                </div>
                <div className="mt-1">
                  Estado:{" "}
                  <strong
                    className={
                      wifiInfo.puntoAccesoActivo
                        ? "text-success"
                        : "text-foreground-secondary"
                    }
                  >
                    {wifiInfo.puntoAccesoActivo ? "Activo" : "Inactivo"}
                  </strong>
                </div>
              </div>
            </div>
          )}

          <form
            onSubmit={guardarWifi}
            className="space-y-4 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
          >
            <h2 className="text-sm font-bold">
              Credenciales Inalámbricas WPA2 (Estación + SoftAP)
            </h2>
            <div className="grid grid-cols-1 gap-3 sm:grid-cols-2 text-xs">
              <div>
                <label className="mb-1 block font-medium">
                  SSID Red Local (STA)
                </label>
                <input
                  type="text"
                  maxLength={31}
                  value={formWifi.ssidSta}
                  onChange={(e) =>
                    setFormWifi({ ...formWifi, ssidSta: e.target.value })
                  }
                  className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block font-medium">
                  Clave WiFi STA (mín. 8 chars WPA2)
                </label>
                <input
                  type="password"
                  maxLength={63}
                  value={formWifi.claveSta}
                  onChange={(e) =>
                    setFormWifi({ ...formWifi, claveSta: e.target.value })
                  }
                  placeholder="••••••••"
                  className="w-full rounded border border-border bg-background px-3 py-1.5"
                />
              </div>
              <div>
                <label className="mb-1 block font-medium">
                  SSID Punto de Acceso (SoftAP)
                </label>
                <input
                  type="text"
                  maxLength={31}
                  required
                  value={formWifi.ssidAp}
                  onChange={(e) =>
                    setFormWifi({ ...formWifi, ssidAp: e.target.value })
                  }
                  className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block font-medium">
                  Clave SoftAP (mín. 8 chars WPA2)
                </label>
                <input
                  type="password"
                  maxLength={63}
                  required
                  value={formWifi.claveAp}
                  onChange={(e) =>
                    setFormWifi({ ...formWifi, claveAp: e.target.value })
                  }
                  placeholder="••••••••"
                  className="w-full rounded border border-border bg-background px-3 py-1.5"
                />
              </div>
            </div>

            <button
              type="submit"
              className="btn btn-solid btn-info px-4 py-2 text-xs font-bold"
            >
              <Save className="h-4 w-4" />
              Guardar Configuración WiFi
            </button>
          </form>
        </div>
      )}

      {/* ================================================================= */}
      {/* PESTAÑA 3: SMTP Y DESTINATARIOS DE CORREO                         */}
      {/* ================================================================= */}
      {tab === "CORREOS" && (
        <div className="space-y-4">
          {/* Servidor SMTP (Solo ADMIN) */}
          {esAdmin && (
            <form
              onSubmit={guardarSmtp}
              className="space-y-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
            >
              <div className="flex items-center justify-between">
                <h2 className="text-sm font-bold">
                  Servidor Saliente SMTP (Alertas por Correo)
                </h2>
                {smtpCfg && (
                  <span className="font-mono text-[10px] text-foreground-secondary">
                    Mod.: {smtpCfg.fechaModificacion || "—"}
                  </span>
                )}
              </div>

              <div className="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-4 text-xs">
                <div>
                  <label className="mb-1 block">Host SMTP</label>
                  <input
                    type="text"
                    maxLength={39}
                    required
                    value={formSmtp.hostSmtp}
                    onChange={(e) =>
                      setFormSmtp({ ...formSmtp, hostSmtp: e.target.value })
                    }
                    placeholder="smtp.gmail.com"
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                  />
                </div>
                <div>
                  <label className="mb-1 block">Puerto</label>
                  <input
                    type="number"
                    min={1}
                    max={65535}
                    required
                    value={formSmtp.puerto}
                    onChange={(e) =>
                      setFormSmtp({
                        ...formSmtp,
                        puerto: Number(e.target.value),
                      })
                    }
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                  />
                </div>
                <div>
                  <label className="mb-1 block">Usuario SMTP</label>
                  <input
                    type="text"
                    maxLength={47}
                    value={formSmtp.usuario}
                    onChange={(e) =>
                      setFormSmtp({ ...formSmtp, usuario: e.target.value })
                    }
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                  />
                </div>
                <div>
                  <label className="mb-1 block">Contraseña / App Password</label>
                  <input
                    type="password"
                    maxLength={159}
                    value={formSmtp.passwordCifrado}
                    onChange={(e) =>
                      setFormSmtp({
                        ...formSmtp,
                        passwordCifrado: e.target.value,
                      })
                    }
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5"
                  />
                </div>
                <div>
                  <label className="mb-1 block">Email Remitente</label>
                  <input
                    type="email"
                    maxLength={47}
                    required
                    value={formSmtp.emailRemitente}
                    onChange={(e) =>
                      setFormSmtp({
                        ...formSmtp,
                        emailRemitente: e.target.value,
                      })
                    }
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                  />
                </div>
                <div>
                  <label className="mb-1 block">Nombre Remitente</label>
                  <input
                    type="text"
                    maxLength={31}
                    value={formSmtp.nombreRemitente}
                    onChange={(e) =>
                      setFormSmtp({
                        ...formSmtp,
                        nombreRemitente: e.target.value,
                      })
                    }
                    className="w-full rounded border border-border bg-background px-2.5 py-1.5"
                  />
                </div>
                <div className="flex items-center gap-4 pt-5">
                  <label className="flex items-center gap-1.5">
                    <input
                      type="checkbox"
                      checked={formSmtp.usarTls}
                      onChange={(e) =>
                        setFormSmtp({ ...formSmtp, usarTls: e.target.checked })
                      }
                    />
                    STARTTLS / TLS
                  </label>
                  <label className="flex items-center gap-1.5">
                    <input
                      type="checkbox"
                      checked={formSmtp.activa}
                      onChange={(e) =>
                        setFormSmtp({ ...formSmtp, activa: e.target.checked })
                      }
                    />
                    SMTP Activo
                  </label>
                </div>
              </div>

              <button
                type="submit"
                className="btn btn-solid btn-primary px-4 py-1.5 text-xs font-bold"
              >
                <Save className="h-3.5 w-3.5" />
                Guardar Servidor SMTP
              </button>
            </form>
          )}

          {/* Lista y CRUD de Destinatarios */}
          <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
            <div className="mb-3 flex items-center justify-between">
              <h2 className="text-sm font-bold">
                Destinatarios de Alertas ({correos.length})
              </h2>
              <button
                type="button"
                className="btn btn-outline px-2.5 py-1 text-xs"
                onClick={() => void cargarCorreosYSmtp()}
              >
                <RefreshCw className="h-3.5 w-3.5" />
              </button>
            </div>

            {esAdmin && (
              <form
                onSubmit={guardarDestinatario}
                className="mb-4 flex flex-wrap items-end gap-2 rounded-lg border border-border bg-background p-3 text-xs"
              >
                <div>
                  <label className="mb-1 block">Nombre</label>
                  <input
                    type="text"
                    maxLength={23}
                    required
                    value={formCorreo.nombre}
                    onChange={(e) =>
                      setFormCorreo({ ...formCorreo, nombre: e.target.value })
                    }
                    className="w-32 rounded border border-border bg-card px-2 py-1"
                  />
                </div>
                <div>
                  <label className="mb-1 block">Apellido</label>
                  <input
                    type="text"
                    maxLength={23}
                    required
                    value={formCorreo.apellido}
                    onChange={(e) =>
                      setFormCorreo({ ...formCorreo, apellido: e.target.value })
                    }
                    className="w-32 rounded border border-border bg-card px-2 py-1"
                  />
                </div>
                <div>
                  <label className="mb-1 block">Correo Electrónico</label>
                  <input
                    type="email"
                    maxLength={47}
                    required
                    value={formCorreo.correoElectronico}
                    onChange={(e) =>
                      setFormCorreo({
                        ...formCorreo,
                        correoElectronico: e.target.value,
                      })
                    }
                    className="w-52 rounded border border-border bg-card px-2 py-1 font-mono"
                  />
                </div>
                <div>
                  <label className="mb-1 block">Filtro de Recepción</label>
                  <select
                    value={formCorreo.tipoRecepcionMensajes}
                    onChange={(e) =>
                      setFormCorreo({
                        ...formCorreo,
                        tipoRecepcionMensajes: Number(
                          e.target.value
                        ) as TipoRecepcionCorreo,
                      })
                    }
                    className="rounded border border-border bg-card px-2 py-1"
                  >
                    <option value={TipoRecepcionCorreo.RECIBIR_TODOS}>
                      RECIBIR_TODOS
                    </option>
                    <option value={TipoRecepcionCorreo.RECIBIR_EVENTOS}>
                      RECIBIR_EVENTOS
                    </option>
                    <option value={TipoRecepcionCorreo.RECIBIR_EVENTOS_CRITICOS}>
                      RECIBIR_EVENTOS_CRITICOS
                    </option>
                  </select>
                </div>
                <label className="flex items-center gap-1.5 pb-1.5">
                  <input
                    type="checkbox"
                    checked={formCorreo.activa}
                    onChange={(e) =>
                      setFormCorreo({ ...formCorreo, activa: e.target.checked })
                    }
                  />
                  Activo
                </label>
                <button
                  type="submit"
                  className="btn btn-solid btn-success px-3 py-1 text-xs font-semibold"
                >
                  <Plus className="h-3.5 w-3.5" />
                  {formCorreo.idCorreo === 0
                    ? "Agregar"
                    : `Actualizar #${formCorreo.idCorreo}`}
                </button>
                {formCorreo.idCorreo !== 0 && (
                  <button
                    type="button"
                    onClick={() =>
                      setFormCorreo({
                        idCorreo: 0,
                        nombre: "",
                        apellido: "",
                        correoElectronico: "",
                        tipoRecepcionMensajes:
                          TipoRecepcionCorreo.RECIBIR_EVENTOS_CRITICOS,
                        activa: true,
                      })
                    }
                    className="btn btn-outline px-2.5 py-1 text-xs"
                  >
                    Cancelar
                  </button>
                )}
              </form>
            )}

            <div className="overflow-x-auto">
              <table className="w-full text-left text-xs">
                <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
                  <tr>
                    <th className="px-3 py-2">ID</th>
                    <th className="px-3 py-2">Destinatario</th>
                    <th className="px-3 py-2">Email</th>
                    <th className="px-3 py-2">Filtro</th>
                    <th className="px-3 py-2">Último Envío</th>
                    <th className="px-3 py-2">Estado</th>
                    {esAdmin && <th className="px-3 py-2 text-right">Acciones</th>}
                  </tr>
                </thead>
                <tbody className="divide-y divide-border">
                  {correos.map((c) => (
                    <tr key={c.idCorreo} className="hover:bg-secondary/40">
                      <td className="px-3 py-2 font-mono">#{c.idCorreo}</td>
                      <td className="px-3 py-2 font-medium">
                        {c.nombre} {c.apellido}
                      </td>
                      <td className="px-3 py-2 font-mono">
                        {c.correoElectronico}
                      </td>
                      <td className="px-3 py-2 text-[11px]">
                        {LABEL_TIPO_RECEPCION[c.tipoRecepcionMensajes] ??
                          c.tipoRecepcionMensajes}
                      </td>
                      <td className="px-3 py-2 text-[11px]">
                        {LABEL_ESTADO_ENVIO[c.ultimoEstadoEnvio] ??
                          c.ultimoEstadoEnvio}
                      </td>
                      <td className="px-3 py-2">
                        <span
                          className={`rounded-full px-2 py-0.5 text-[10px] font-semibold ${
                            c.activa
                              ? "bg-alert-success-bg text-alert-success-icon"
                              : "bg-alert-inactive-bg text-alert-inactive-icon"
                          }`}
                        >
                          {c.activa ? "ACTIVO" : "INACTIVO"}
                        </span>
                      </td>
                      {esAdmin && (
                        <td className="px-3 py-2 text-right">
                          <div className="inline-flex items-center gap-1">
                            <button
                              className="btn btn-icon btn-info"
                              onClick={() =>
                                setFormCorreo({
                                  idCorreo: c.idCorreo,
                                  nombre: c.nombre,
                                  apellido: c.apellido,
                                  correoElectronico: c.correoElectronico,
                                  tipoRecepcionMensajes:
                                    c.tipoRecepcionMensajes,
                                  activa: c.activa,
                                })
                              }
                              title="Editar"
                            >
                              <Edit2 className="h-3.5 w-3.5" />
                            </button>
                            <button
                              className="btn btn-icon btn-error"
                              onClick={() =>
                                void eliminarDestinatario(c.idCorreo)
                              }
                              title="Eliminar"
                            >
                              <Trash2 className="h-3.5 w-3.5" />
                            </button>
                          </div>
                        </td>
                      )}
                    </tr>
                  ))}
                  {correos.length === 0 && (
                    <tr>
                      <td
                        colSpan={esAdmin ? 7 : 6}
                        className="px-3 py-6 text-center text-foreground-secondary"
                      >
                        No hay destinatarios configurados.
                      </td>
                    </tr>
                  )}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}

      {/* ================================================================= */}
      {/* PESTAÑA 4: SERVIDORES NTP                                         */}
      {/* ================================================================= */}
      {tab === "NTP" && (
        <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
          <div className="mb-3 flex items-center justify-between">
            <h2 className="text-sm font-bold">
              Servidores de Sincronización Horaria NTP ({servidoresNtp.length})
            </h2>
            <button
              type="button"
              className="btn btn-outline px-2.5 py-1 text-xs"
              onClick={() => void cargarNtp()}
            >
              <RefreshCw className="h-3.5 w-3.5" />
            </button>
          </div>

          {esAdmin && (
            <form
              onSubmit={guardarNtp}
              className="mb-4 flex flex-wrap items-end gap-2 rounded-lg border border-border bg-background p-3 text-xs"
            >
              <div>
                <label className="mb-1 block">Host / IP NTP</label>
                <input
                  type="text"
                  maxLength={39}
                  required
                  value={formNtp.server}
                  onChange={(e) =>
                    setFormNtp({ ...formNtp, server: e.target.value })
                  }
                  placeholder="pool.ntp.org"
                  className="w-56 rounded border border-border bg-card px-2.5 py-1 font-mono"
                />
              </div>
              <div>
                <label className="mb-1 block">Prioridad (1..10)</label>
                <input
                  type="number"
                  min={1}
                  max={10}
                  required
                  value={formNtp.prioridad}
                  onChange={(e) =>
                    setFormNtp({
                      ...formNtp,
                      prioridad: Number(e.target.value),
                    })
                  }
                  className="w-24 rounded border border-border bg-card px-2.5 py-1 font-mono"
                />
              </div>
              <label className="flex items-center gap-1.5 pb-1.5">
                <input
                  type="checkbox"
                  checked={formNtp.activa}
                  onChange={(e) =>
                    setFormNtp({ ...formNtp, activa: e.target.checked })
                  }
                />
                Activo
              </label>
              <button
                type="submit"
                className="btn btn-solid btn-primary px-3 py-1 text-xs font-semibold"
              >
                <Plus className="h-3.5 w-3.5" />
                {formNtp.idServer === 0
                  ? "Agregar Servidor"
                  : `Actualizar #${formNtp.idServer}`}
              </button>
              {formNtp.idServer !== 0 && (
                <button
                  type="button"
                  onClick={() =>
                    setFormNtp({
                      idServer: 0,
                      server: "",
                      prioridad: 1,
                      activa: true,
                    })
                  }
                  className="btn btn-outline px-2.5 py-1 text-xs"
                >
                  Cancelar
                </button>
              )}
            </form>
          )}

          <div className="overflow-x-auto">
            <table className="w-full text-left text-xs">
              <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
                <tr>
                  <th className="px-3 py-2">ID</th>
                  <th className="px-3 py-2">Servidor NTP</th>
                  <th className="px-3 py-2">Prioridad</th>
                  <th className="px-3 py-2">Estado</th>
                  {esAdmin && <th className="px-3 py-2 text-right">Acciones</th>}
                </tr>
              </thead>
              <tbody className="divide-y divide-border">
                {servidoresNtp.map((ntp) => (
                  <tr key={ntp.idServer} className="hover:bg-secondary/40">
                    <td className="px-3 py-2 font-mono">#{ntp.idServer}</td>
                    <td className="px-3 py-2 font-mono font-bold">
                      {ntp.server}
                    </td>
                    <td className="px-3 py-2 font-mono">{ntp.prioridad}</td>
                    <td className="px-3 py-2">
                      <span
                        className={`rounded-full px-2 py-0.5 text-[10px] font-semibold ${
                          ntp.activa
                            ? "bg-alert-success-bg text-alert-success-icon"
                            : "bg-alert-inactive-bg text-alert-inactive-icon"
                        }`}
                      >
                        {ntp.activa ? "ACTIVO" : "INACTIVO"}
                      </span>
                    </td>
                    {esAdmin && (
                      <td className="px-3 py-2 text-right">
                        <div className="inline-flex items-center gap-1">
                          <button
                            className="btn btn-icon btn-info"
                            onClick={() =>
                              setFormNtp({
                                idServer: ntp.idServer,
                                server: ntp.server,
                                prioridad: ntp.prioridad,
                                activa: ntp.activa,
                              })
                            }
                            title="Editar"
                          >
                            <Edit2 className="h-3.5 w-3.5" />
                          </button>
                          <button
                            className="btn btn-icon btn-error"
                            onClick={() => void eliminarNtp(ntp.idServer)}
                            title="Eliminar"
                          >
                            <Trash2 className="h-3.5 w-3.5" />
                          </button>
                        </div>
                      </td>
                    )}
                  </tr>
                ))}
                {servidoresNtp.length === 0 && (
                  <tr>
                    <td
                      colSpan={esAdmin ? 5 : 4}
                      className="px-3 py-6 text-center text-foreground-secondary"
                    >
                      Sin servidores NTP configurados.
                    </td>
                  </tr>
                )}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
}