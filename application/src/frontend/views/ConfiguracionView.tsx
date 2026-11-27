import React, { useCallback, useEffect, useState } from "react";
import {
  Settings,
  Globe,
  Wifi,
  Mail,
  Clock,
  Save,
  Plus,
  Trash2,
  Edit2,
  RefreshCw,
  Loader2,
  Send,
  CheckCircle2,
  XCircle,
  FileSpreadsheet,
  HelpCircle,
  RotateCw,
  Unlink,
  Cloud,
  Power,
} from "lucide-react";
import { toast } from "react-toastify";
import { useAuth } from "../contexts/AuthContext";
import { client, safeRpc } from "../lib/service";
import { formatearEpochSegundos } from "../lib/format";
import { LABEL_ESTADO_ENVIO, LABEL_TIPO_RECEPCION } from "../lib/labels";
import { sufijoFechaArchivo } from "../lib/exportCsv";
import { exportarTablaExcel } from "../lib/exportExcel";
import { NumericInput } from "../components/ui/NumericInput";
import { PasswordInput } from "../components/ui/PasswordInput";
import {
  CalibracionAmbienteDTO,
  ConfiguracionSistemaDTO,
  ConfiguracionSmtpDTO,
  ConfiguracionWiFiCliente,
  CorreoItemDTO,
  RolUsuario,
  ServidorNtpItemDTO,
  TipoRecepcionCorreo,
  ResultadoEnvioSmtpDTO,
  ConfiguracionRelayDTO,
  EstadoRelaySistemaDTO,
  EstadoRelay,
  ErrorRelay,
  EstadoRelayLocalDTO,
} from "#types/rpc.types";
import { FormulaTooltip } from "../components/ui/FormulaTooltip";
import {
  estimarEbullicionDesdePresion,
  estimarPresionDesdeAltura,
} from "../lib/calibracionModels";
import { DataTable, DataTableColumn } from "#components/ui/DataTable";

export type TabConfiguracion =
  | "SISTEMA"
  | "AMBIENTE"
  | "WIFI"
  | "CORREOS"
  | "NTP";

function ParamHelpTooltip({
  titulo,
  texto,
}: {
  titulo: string;
  texto: string;
}) {
  const [abierto, setAbierto] = useState(false);

  return (
    <span
      className="relative inline-flex items-center"
      onMouseEnter={() => setAbierto(true)}
      onMouseLeave={() => setAbierto(false)}
    >
      <button
        type="button"
        onClick={() => setAbierto((v) => !v)}
        onBlur={() => setAbierto(false)}
        aria-label={`Ayuda sobre ${titulo}`}
        className="inline-flex h-4 w-4 items-center justify-center rounded-full text-foreground-secondary transition-colors hover:bg-secondary hover:text-primary"
      >
        <HelpCircle className="h-3.5 w-3.5" />
      </button>
      {abierto && (
        <div
          role="tooltip"
          className="absolute bottom-full left-0 z-40 mb-1.5 w-64 rounded-lg border border-border bg-card p-2.5 text-[11px] leading-relaxed font-normal text-card-foreground shadow-lg"
        >
          <div className="mb-0.5 font-semibold text-primary">{titulo}</div>
          <p className="text-foreground-secondary">{texto}</p>
        </div>
      )}
    </span>
  );
}

const LABEL_ESTADO_RELAY: Record<EstadoRelay, string> = {
  [EstadoRelay.DESHABILITADO]: "Deshabilitado",
  [EstadoRelay.SIN_WIFI_STA]: "Esperando WiFi STA",
  [EstadoRelay.DESCONECTADO]: "Desconectado (reintentando)",
  [EstadoRelay.ESPERANDO_CHALLENGE]: "Conectando (reto)",
  [EstadoRelay.ESPERANDO_AUTH_OK]: "Autenticando HMAC",
  [EstadoRelay.ACTIVO]: "Túnel Activo",
};

const LABEL_ERROR_RELAY: Record<ErrorRelay, string> = {
  [ErrorRelay.NINGUNO]: "Sin errores",
  [ErrorRelay.CONEXION_FALLIDA]: "No se pudo conectar al VPS",
  [ErrorRelay.AUTH_RECHAZADA]: "Auth rechazada (revisá Node ID / Secreto)",
  [ErrorRelay.TIMEOUT_HANDSHAKE]: "Timeout en handshake",
  [ErrorRelay.TIMEOUT_KEEPALIVE]: "Timeout de keepalive",
  [ErrorRelay.TRAMA_INVALIDA]: "Trama Mux inválida",
  [ErrorRelay.SOCKET_CAIDO]: "Socket caído",
};

export function ConfiguracionView({
  tabInicial = "SISTEMA",
}: {
  tabInicial?: TabConfiguracion;
}) {
  const { rol } = useAuth();
  const esAdmin = rol === RolUsuario.ADMIN;

  const [tab, setTab] = useState<TabConfiguracion>(tabInicial);
  const [cargando, setCargando] = useState(false);

  useEffect(() => {
    setTab(tabInicial === "WIFI" && !esAdmin ? "SISTEMA" : tabInicial);
  }, [tabInicial, esAdmin]);

  // --- Pestaña 1: Sistema y Retención ---
  const [cfgSistema, setCfgSistema] = useState<ConfiguracionSistemaDTO | null>(
    null
  );
  const [formSistema, setFormSistema] = useState({
    zonaHorariaMin: -180,
    retencionCensoDias: 30,
    retencionEventosDias: 60,
    retencionAuditoriaDias: 90,
    sdLibreMinimoKb: 2048,
    correoCooldownS: 300,
    activa: true,
  });

  // --- Pestaña 2: Calibración Ambiental Global ---
  const [ambiente, setAmbiente] = useState<CalibracionAmbienteDTO | null>(null);
  const [formAmbiente, setFormAmbiente] = useState({
    altura: 115,
    presionAtmosferica: 1013.25,
    tempCongelacionRef: 0,
    tempEbullicionRef: 100,
    activa: true,
  });

  // --- Pestaña 3: WiFi STA + SoftAP separados (Solo ADMIN) ---
  const [wifiInfo, setWifiInfo] = useState<ConfiguracionWiFiCliente | null>(
    null
  );
  const [formWifiSta, setFormWifiSta] = useState({
    ssidSta: "",
    claveSta: "",
  });
  const [formWifiAp, setFormWifiAp] = useState({
    ssidAp: "",
    claveAp: "",
  });
  const [accionWifi, setAccionWifi] = useState<
    null | "RECONECTAR" | "STA" | "AP" | "LIMPIAR_STA"
  >(null);

  // --- Túnel Remoto (Reverse TCP Relay) ---
  const [relayCfg, setRelayCfg] = useState<ConfiguracionRelayDTO | null>(null);
  const [relayEstado, setRelayEstado] = useState<EstadoRelaySistemaDTO | null>(
    null
  );
  const [formRelay, setFormRelay] = useState({
    hostRelay: "",
    puerto: 9443,
    nodeId: "ESP32-UNTDF-0001",
    secreto: "",
    activa: false,
  });
  const [guardandoRelay, setGuardandoRelay] = useState(false);

  const [relayLocal, setRelayLocal] = useState<EstadoRelayLocalDTO | null>(
    null
  );
  const [cambiandoRelayLocal, setCambiandoRelayLocal] = useState(false);

  // --- Pestaña 4: SMTP y Destinatarios de Correo ---
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
  const [destinoPruebaSmtp, setDestinoPruebaSmtp] = useState("");
  const [probandoSmtp, setProbandoSmtp] = useState(false);
  const [resultadoPruebaSmtp, setResultadoPruebaSmtp] =
    useState<ResultadoEnvioSmtpDTO | null>(null);
  const [formSmtp, setFormSmtp] = useState({
    hostSmtp: "",
    puerto: 465,
    usarTls: true,
    usuario: "",
    passwordCifrado: "",
    emailRemitente: "",
    nombreRemitente: "ESP32 UNTDF",
    activa: true,
  });

  // --- Pestaña 5: Servidores NTP ---
  const [servidoresNtp, setServidoresNtp] = useState<ServidorNtpItemDTO[]>([]);
  const [formNtp, setFormNtp] = useState({
    idServer: 0,
    server: "",
    prioridad: 1,
    activa: true,
  });

  const cargarSistema = useCallback(async () => {
    setCargando(true);
    const cfg = await safeRpc(
      () => client.obtenerConfiguracionSistema(),
      "obtenerConfiguracionSistema"
    );
    setCargando(false);
    if (cfg) {
      setCfgSistema(cfg);
      setFormSistema({
        zonaHorariaMin: cfg.zonaHorariaMin,
        retencionCensoDias: cfg.retencionCensoDias,
        retencionEventosDias: cfg.retencionEventosDias,
        retencionAuditoriaDias: cfg.retencionAuditoriaDias,
        sdLibreMinimoKb: cfg.sdLibreMinimoKb,
        correoCooldownS: cfg.correoCooldownS,
        activa: cfg.activa,
      });
    }
  }, []);

  const cargarAmbiente = useCallback(async () => {
    setCargando(true);
    const amb = await safeRpc(
      () => client.obtenerCalibracionAmbiente(),
      "obtenerCalibracionAmbiente"
    );
    setCargando(false);
    if (amb) {
      setAmbiente(amb);
      setFormAmbiente({
        altura: amb.altura,
        presionAtmosferica: amb.presionAtmosferica,
        tempCongelacionRef: amb.tempCongelacionRef,
        tempEbullicionRef: amb.tempEbullicionRef,
        activa: amb.activa,
      });
    }
  }, []);

  const cargarWifi = useCallback(
    async (forzarSinCache = false) => {
      if (!esAdmin) return;
      setCargando(true);
      if (forzarSinCache) {
        await safeRpc(() => client.invalidarCache(), "invalidarCache", {
          silent: true,
        });
      }
      const [w, rCfg, rEst, rLocal] = await Promise.all([
        safeRpc(
          () => client.obtenerConfiguracionWiFi(),
          "obtenerConfiguracionWiFi"
        ),
        safeRpc(
          () => client.obtenerConfiguracionRelay(),
          "obtenerConfiguracionRelay",
          { silent: true }
        ),
        safeRpc(() => client.obtenerEstadoRelay(), "obtenerEstadoRelay", {
          silent: true,
        }),
        safeRpc(
          () => client.obtenerEstadoRelayLocal(),
          "obtenerEstadoRelayLocal",
          { silent: true }
        ),
      ]);
      setCargando(false);
      if (w) {
        setWifiInfo(w);
        setFormWifiSta((prev) => ({ ...prev, ssidSta: w.ssidSta }));
        setFormWifiAp((prev) => ({ ...prev, ssidAp: w.ssidAp }));
      }
      if (rCfg) {
        setRelayCfg(rCfg);
        setFormRelay({
          hostRelay: rCfg.hostRelay,
          puerto: rCfg.puerto || 9443,
          nodeId: rCfg.nodeId || "ESP32-UNTDF-0001",
          secreto: "",
          activa: rCfg.activa,
        });
      }
      if (rEst) {
        setRelayEstado(rEst);
      }
      if (rLocal) {
        setRelayLocal(rLocal);
      }
    },
    [esAdmin]
  );

  const cargarCorreosYSmtp = useCallback(async () => {
    setCargando(true);
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
    setCargando(false);
  }, [esAdmin]);

  const cargarNtp = useCallback(async () => {
    setCargando(true);
    const lista = await safeRpc(
      () => client.listarServidoresNtp(),
      "listarServidoresNtp"
    );
    setCargando(false);
    if (lista) setServidoresNtp(lista);
  }, []);

  useEffect(() => {
    if (tab === "SISTEMA") void cargarSistema();
    if (tab === "AMBIENTE") void cargarAmbiente();
    if (tab === "WIFI") void cargarWifi(false);
    if (tab === "CORREOS") void cargarCorreosYSmtp();
    if (tab === "NTP") void cargarNtp();
  }, [
    tab,
    cargarSistema,
    cargarAmbiente,
    cargarWifi,
    cargarCorreosYSmtp,
    cargarNtp,
  ]);

  // Handlers de Guardado (ADMIN)
  const guardarSistema = async (e: React.FormEvent) => {
    e.preventDefault();
    const res = await safeRpc(
      () =>
        client.actualizarConfiguracionSistema({
          zonaHorariaMin: Math.round(formSistema.zonaHorariaMin),
          retencionCensoDias: Math.round(formSistema.retencionCensoDias),
          retencionEventosDias: Math.round(formSistema.retencionEventosDias),
          retencionAuditoriaDias: Math.round(
            formSistema.retencionAuditoriaDias
          ),
          sdLibreMinimoKb: Math.round(formSistema.sdLibreMinimoKb),
          correoCooldownS: Math.round(formSistema.correoCooldownS),
          activa: formSistema.activa,
        }),
      "actualizarConfiguracionSistema",
      { mensajeExito: "Configuración general del sistema guardada." }
    );
    if (res) setCfgSistema(res);
  };

  const guardarAmbiente = async (e: React.FormEvent) => {
    e.preventDefault();
    const res = await safeRpc(
      () =>
        client.actualizarCalibracionAmbiente({
          altura: formAmbiente.altura,
          presionAtmosferica: formAmbiente.presionAtmosferica,
          tempCongelacionRef: formAmbiente.tempCongelacionRef,
          tempEbullicionRef: formAmbiente.tempEbullicionRef,
          activa: formAmbiente.activa,
        }),
      "actualizarCalibracionAmbiente",
      { mensajeExito: "Calibración ambiental global actualizada en el ESP32." }
    );
    if (res) setAmbiente(res);
  };

  const aplicarEstimacionDesdeAltura = () => {
    const pEst = estimarPresionDesdeAltura(formAmbiente.altura);
    const tEbull = estimarEbullicionDesdePresion(pEst);
    setFormAmbiente((prev) => ({
      ...prev,
      presionAtmosferica: pEst,
      tempEbullicionRef: tEbull,
    }));
  };

  const aplicarEstimacionDesdePresion = () => {
    const tEbull = estimarEbullicionDesdePresion(
      formAmbiente.presionAtmosferica
    );
    setFormAmbiente((prev) => ({ ...prev, tempEbullicionRef: tEbull }));
  };

  // --- Handlers WiFi desacoplados (STA / SoftAP / Reconectar STA) ---
  const reconectarWifiSta = async () => {
    setAccionWifi("RECONECTAR");
    const res = await safeRpc(
      () => client.actualizarConfiguracionWiFi("", "", "", ""),
      "reconectarWifiSta"
    );
    setAccionWifi(null);
    if (res) {
      setWifiInfo(res);
      if (res.conectadoARouter) {
        toast.success(`Conectado al router (${res.ssidSta} · IP ${res.ipSta}).`);
      } else {
        toast.warning(
          res.ssidSta
            ? `El ESP32 no logró conectarse a "${res.ssidSta}". Verificá alcance o contraseña.`
            : "No hay una red WiFi STA configurada todavía."
        );
      }
    }
  };

  const guardarWifiSta = async (e: React.FormEvent) => {
    e.preventDefault();
    const ssid = formWifiSta.ssidSta.trim();
    if (!ssid) {
      toast.warning("Ingresá el SSID de la red local (router) para STA.");
      return;
    }
    if (formWifiSta.claveSta.length > 0 && formWifiSta.claveSta.length < 8) {
      toast.warning(
        "La clave WiFi STA debe tener al menos 8 caracteres (WPA2), o dejala vacía para conservar la actual."
      );
      return;
    }

    setAccionWifi("STA");
    const res = await safeRpc(
      () =>
        client.actualizarConfiguracionWiFi(ssid, formWifiSta.claveSta, "", ""),
      "actualizarWifiSta"
    );
    setAccionWifi(null);
    if (res) {
      setWifiInfo(res);
      setFormWifiSta({ ssidSta: res.ssidSta, claveSta: "" });
      if (res.conectadoARouter) {
        toast.success(`WiFi STA guardado y conectado (IP ${res.ipSta}).`);
      } else {
        toast.info(
          "Credenciales STA guardadas (el SoftAP no fue modificado), pero el router no respondió en este intento."
        );
      }
    }
  };

  const desvincularWifiSta = async () => {
    if (
      !window.confirm(
        "¿Olvidar las credenciales del router (STA) y desconectar? El Punto de Acceso (SoftAP) seguirá activo."
      )
    ) {
      return;
    }
    setAccionWifi("LIMPIAR_STA");
    const res = await safeRpc(
      () => client.actualizarConfiguracionWiFi("__CLEAR__", "", "", ""),
      "desvincularWifiSta",
      { mensajeExito: "Red WiFi STA desvinculada. SoftAP sigue activo." }
    );
    setAccionWifi(null);
    if (res) {
      setWifiInfo(res);
      setFormWifiSta({ ssidSta: "", claveSta: "" });
    }
  };

  const guardarWifiAp = async (e: React.FormEvent) => {
    e.preventDefault();
    const ssid = formWifiAp.ssidAp.trim();
    if (!ssid) {
      toast.warning("El SSID del Punto de Acceso (SoftAP) no puede estar vacío.");
      return;
    }
    if (formWifiAp.claveAp.length > 0 && formWifiAp.claveAp.length < 8) {
      toast.warning(
        "La clave del SoftAP debe tener al menos 8 caracteres (WPA2), o dejala vacía para conservar la actual."
      );
      return;
    }

    setAccionWifi("AP");
    const res = await safeRpc(
      () =>
        client.actualizarConfiguracionWiFi("", "", ssid, formWifiAp.claveAp),
      "actualizarWifiAp",
      {
        mensajeExito:
          "Punto de Acceso (SoftAP) actualizado sin modificar la red STA.",
      }
    );
    setAccionWifi(null);
    if (res) {
      setWifiInfo(res);
      setFormWifiAp({ ssidAp: res.ssidAp, claveAp: "" });
    }
  };

  const guardarRelay = async (e: React.FormEvent) => {
    e.preventDefault();
    const host = formRelay.hostRelay.trim();
    const nodeId = formRelay.nodeId.trim() || "ESP32-UNTDF-0001";
    const tieneSecretoPrevio = Boolean(relayCfg?.secreto);

    if (formRelay.activa && !host) {
      toast.warning("Ingresá la IP o dominio del VPS Relay antes de activarlo.");
      return;
    }
    if (
      formRelay.activa &&
      formRelay.secreto.length > 0 &&
      formRelay.secreto.length < 16
    ) {
      toast.warning("El secreto del Relay debe tener al menos 16 caracteres.");
      return;
    }
    if (formRelay.activa && !formRelay.secreto && !tieneSecretoPrevio) {
      toast.warning("Ingresá un secreto compartido (mín. 16 caracteres).");
      return;
    }

    setGuardandoRelay(true);
    const res = await safeRpc(
      () =>
        client.actualizarConfiguracionRelay({
          hostRelay: host,
          puerto: Math.round(formRelay.puerto),
          nodeId,
          secreto: formRelay.secreto,
          activa: formRelay.activa,
        }),
      "actualizarConfiguracionRelay",
      {
        mensajeExito: formRelay.activa
          ? "Túnel Relay guardado y activado en caliente."
          : "Túnel Relay desactivado.",
      }
    );
    setGuardandoRelay(false);

    if (res) {
      setRelayCfg(res);
      setFormRelay({
        hostRelay: res.hostRelay,
        puerto: res.puerto || 9443,
        nodeId: res.nodeId || "ESP32-UNTDF-0001",
        secreto: "",
        activa: res.activa,
      });
      setTimeout(async () => {
        const [est, estLocal] = await Promise.all([
          safeRpc(() => client.obtenerEstadoRelay(), "obtenerEstadoRelay", {
            silent: true,
          }),
          safeRpc(
            () => client.obtenerEstadoRelayLocal(),
            "obtenerEstadoRelayLocal",
            { silent: true }
          ),
        ]);
        if (est) setRelayEstado(est);
        if (estLocal) setRelayLocal(estLocal);
      }, 1200);
    }
  };

  const alternarRelayLocal = async () => {
    setCambiandoRelayLocal(true);
    if (relayLocal?.activo) {
      const est = await safeRpc(
        () => client.detenerRelayLocal(),
        "detenerRelayLocal",
        { mensajeExito: "Servidor Relay local de prueba detenido." }
      );
      if (est) setRelayLocal(est);
    } else {
      const secretoUsar =
        formRelay.secreto.trim() ||
        relayCfg?.secreto?.trim() ||
        "prueba_local_untdf_2026_1234";

      const est = await safeRpc(
        () =>
          client.iniciarRelayLocal({
            puertoTunel: Math.round(formRelay.puerto || 9443),
            puertoClientes: 8443,
            nodeId: formRelay.nodeId.trim() || "ESP32-UNTDF-0001",
            secreto: secretoUsar,
          }),
        "iniciarRelayLocal"
      );
      if (est) {
        setRelayLocal(est);
        setFormRelay((prev) => ({
          ...prev,
          hostRelay: prev.hostRelay.trim() || est.ipLan,
          secreto: prev.secreto || (relayCfg?.secreto ? "" : secretoUsar),
        }));
        toast.success(
          `Relay local encendido en ${est.ipLan}:${est.puertoTunel} (clientes en :${est.puertoClientes}).`
        );
      }
    }
    setCambiandoRelayLocal(false);
  };

  const guardarSmtp = async (e: React.FormEvent) => {
    e.preventDefault();
    const res = await safeRpc(
      () =>
        client.actualizarConfiguracionSmtp({
          hostSmtp: formSmtp.hostSmtp.trim(),
          puerto: Math.round(formSmtp.puerto),
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

  const probarEnvioSmtp = async () => {
    setProbandoSmtp(true);
    setResultadoPruebaSmtp(null);
    const res = await safeRpc(
      () => client.enviarCorreoPruebaSmtp(destinoPruebaSmtp.trim()),
      "enviarCorreoPruebaSmtp"
    );
    setProbandoSmtp(false);
    if (res) {
      setResultadoPruebaSmtp(res);
      void cargarCorreosYSmtp();
    }
  };

  const despacharPendientesAhora = async () => {
    setProbandoSmtp(true);
    setResultadoPruebaSmtp(null);
    const res = await safeRpc(
      () => client.despacharAlertasCorreoAhora(),
      "despacharAlertasCorreoAhora"
    );
    setProbandoSmtp(false);
    if (res) {
      setResultadoPruebaSmtp(res);
      void cargarCorreosYSmtp();
    }
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
          prioridad: Math.round(formNtp.prioridad),
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

  const exportarCorreosExcel = async () => {
    if (correos.length === 0) {
      toast.info("No hay destinatarios cargados para exportar.");
      return;
    }
    await exportarTablaExcel({
      nombreArchivo: `destinatarios_correos_${sufijoFechaArchivo()}.xlsx`,
      nombreHoja: "Correos",
      titulo: "Destinatarios de Alertas por Correo (SMTP)",
      subtitulo: `${correos.length} destinatarios registrados`,
      columnas: [
        { header: "ID", width: 10, numFmt: "0" },
        { header: "Nombre", width: 18 },
        { header: "Apellido", width: 18 },
        { header: "Correo Electrónico", width: 32 },
        { header: "Filtro de Recepción", width: 24 },
        { header: "Último Envío", width: 16 },
        { header: "Estado", width: 12 },
      ],
      filas: correos.map((c) => [
        c.idCorreo,
        c.nombre,
        c.apellido,
        c.correoElectronico,
        LABEL_TIPO_RECEPCION[c.tipoRecepcionMensajes] ??
          String(c.tipoRecepcionMensajes),
        LABEL_ESTADO_ENVIO[c.ultimoEstadoEnvio] ?? String(c.ultimoEstadoEnvio),
        c.activa ? "ACTIVO" : "INACTIVO",
      ]),
    });
    toast.success(`Excel de correos exportado (${correos.length} filas).`);
  };

  const exportarNtpExcel = async () => {
    if (servidoresNtp.length === 0) {
      toast.info("No hay servidores NTP cargados para exportar.");
      return;
    }
    await exportarTablaExcel({
      nombreArchivo: `servidores_ntp_${sufijoFechaArchivo()}.xlsx`,
      nombreHoja: "Servidores NTP",
      titulo: "Servidores de Sincronización Horaria NTP",
      subtitulo: `${servidoresNtp.length} servidores configurados`,
      columnas: [
        { header: "ID", width: 10, numFmt: "0" },
        { header: "Servidor NTP", width: 30 },
        { header: "Prioridad", width: 14, numFmt: "0" },
        { header: "Estado", width: 12 },
      ],
      filas: servidoresNtp.map((n) => [
        n.idServer,
        n.server,
        n.prioridad,
        n.activa ? "ACTIVO" : "INACTIVO",
      ]),
    });
    toast.success(
      `Excel de servidores NTP exportado (${servidoresNtp.length} filas).`
    );
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
            <div className="flex items-center gap-2">
              <h1 className="text-lg font-bold">
                Configuración del Sistema, Ambiente, Red y Notificaciones
              </h1>
              {cargando && (
                <Loader2 className="h-4 w-4 animate-spin text-primary" />
              )}
            </div>
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
            Sistema
          </button>
          <button
            type="button"
            onClick={() => setTab("AMBIENTE")}
            className={`flex items-center gap-1.5 rounded-md px-3 py-1.5 font-semibold ${
              tab === "AMBIENTE"
                ? "bg-card text-foreground shadow-xs"
                : "text-foreground-secondary hover:text-foreground"
            }`}
          >
            <Globe className="h-3.5 w-3.5" />
            Calibración Ambiental
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

      {/* PESTAÑA 1: SISTEMA Y RETENCIÓN */}
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

          <div className="grid grid-cols-1 gap-3 text-xs sm:grid-cols-2 lg:grid-cols-3">
            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Zona Horaria (minutos UTC)</span>
                <ParamHelpTooltip
                  titulo="Desplazamiento horario respecto de UTC"
                  texto="Cantidad de minutos que el reloj del ESP32 suma o resta a la hora UTC recibida por NTP. Para Argentina (UTC-3) corresponde -180. Se aplica a las fechas de auditorías y correos."
                />
              </label>
              <NumericInput
                value={formSistema.zonaHorariaMin}
                onChange={(v) =>
                  setFormSistema((p) => ({ ...p, zonaHorariaMin: v }))
                }
                decimals={0}
                min={-720}
                max={840}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Retención Tabla Censo (días)</span>
                <ParamHelpTooltip
                  titulo="Antigüedad máxima de lecturas térmicas"
                  texto="Cantidad de días que se conservan las mediciones en la tabla Censo. Cuando la purga automática está activa, los registros más antiguos que este plazo se eliminan y compactan en la MicroSD."
                />
              </label>
              <NumericInput
                value={formSistema.retencionCensoDias}
                onChange={(v) =>
                  setFormSistema((p) => ({ ...p, retencionCensoDias: v }))
                }
                decimals={0}
                allowNegative={false}
                min={1}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Retención Eventos Sensores (días)</span>
                <ParamHelpTooltip
                  titulo="Historial de alarmas y transiciones DS18B20"
                  texto="Plazo en días durante el cual se guardan los eventos de AuditoriaSensores (alarmas de temperatura, desconexiones y errores de bus) antes de ser purgados automáticamente."
                />
              </label>
              <NumericInput
                value={formSistema.retencionEventosDias}
                onChange={(v) =>
                  setFormSistema((p) => ({ ...p, retencionEventosDias: v }))
                }
                decimals={0}
                allowNegative={false}
                min={1}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Retención Auditoría Auth/Sistema (días)</span>
                <ParamHelpTooltip
                  titulo="Trazabilidad de seguridad y BIOS"
                  texto="Días de retención para AuditoriaAuth (logins, cambios de rol y cadena HMAC-64) y AuditoriaSistema (arranques, heap y salud de MicroSD). Se recomienda un valor mayor al de Censo."
                />
              </label>
              <NumericInput
                value={formSistema.retencionAuditoriaDias}
                onChange={(v) =>
                  setFormSistema((p) => ({ ...p, retencionAuditoriaDias: v }))
                }
                decimals={0}
                allowNegative={false}
                min={1}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Espacio Libre Mínimo MicroSD (KB)</span>
                <ParamHelpTooltip
                  titulo="Reserva de seguridad en tarjeta MicroSD"
                  texto="Umbral mínimo de espacio libre (en KB, 1024 KB = 1 MB). Si la MicroSD baja de este margen, el sistema alerta y prioriza liberar registros antiguos para evitar quedarse sin almacenamiento."
                />
              </label>
              <NumericInput
                value={formSistema.sdLibreMinimoKb}
                onChange={(v) =>
                  setFormSistema((p) => ({ ...p, sdLibreMinimoKb: v }))
                }
                decimals={0}
                allowNegative={false}
                min={128}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Cooldown Envío de Correos (segundos)</span>
                <ParamHelpTooltip
                  titulo="Intervalo mínimo entre envíos SMTP automáticos"
                  texto="Tiempo de espera (en segundos) que el ESP32 respeta antes de volver a despachar un nuevo lote de correos de alerta o reintentar avisos fallidos. Evita saturar Gmail si una alarma oscila (ej. 300 s = 5 min; 0 = sin espera)."
                />
              </label>
              <NumericInput
                value={formSistema.correoCooldownS}
                onChange={(v) =>
                  setFormSistema((p) => ({ ...p, correoCooldownS: v }))
                }
                decimals={0}
                allowNegative={false}
                min={0}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>
          </div>

          <div className="flex items-center gap-2 text-xs">
            <label className="flex items-center gap-2">
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
            <ParamHelpTooltip
              titulo="Mantenimiento automático de MicroSD"
              texto="Si está activado, el ESP32 ejecuta periódicamente la limpieza de registros vencidos según los días de retención configurados arriba. Si se desactiva, solo se eliminan datos de forma manual."
            />
          </div>

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

      {/* PESTAÑA 2: CALIBRACIÓN AMBIENTAL GLOBAL */}
      {tab === "AMBIENTE" && (
        <form
          onSubmit={guardarAmbiente}
          className="space-y-4 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
        >
          {ambiente && (
            <div className="text-right font-mono text-[11px] text-foreground-secondary">
              Última mod.: {formatearEpochSegundos(ambiente.fechaModificacion)}
            </div>
          )}

          <div className="grid grid-cols-1 gap-3 text-xs sm:grid-cols-2 lg:grid-cols-4">
            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Altura s.n.m. (m)</span>
              </label>
              <NumericInput
                value={formAmbiente.altura}
                onChange={(v) => setFormAmbiente((p) => ({ ...p, altura: v }))}
                decimals={2}
                min={-500}
                max={9000}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Presión Atmosférica (hPa)</span>
                <FormulaTooltip
                  titulo="Fórmula Barométrica ISA"
                  archivoModelo="ModeloAtmosfericoBarometrico.hpp"
                  ecuacion={"P(h) = 1013.25 · (1 − 2.25577×10⁻⁵ · h)^5.25588"}
                  sustitucion={`Con h = ${formAmbiente.altura} m → P ≈ ${estimarPresionDesdeAltura(formAmbiente.altura).toFixed(2)} hPa`}
                  descripcion="Estima la presión atmosférica media a partir de la altitud sobre el nivel del mar (m s.n.m.)."
                />
              </label>
              <NumericInput
                value={formAmbiente.presionAtmosferica}
                onChange={(v) =>
                  setFormAmbiente((p) => ({ ...p, presionAtmosferica: v }))
                }
                decimals={2}
                allowNegative={false}
                min={300}
                max={1100}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Ref. Congelación (°C)</span>
                <FormulaTooltip
                  titulo="Punto Fijo de Fusión del Agua"
                  archivoModelo="ModeloAtmosfericoBarometrico.hpp"
                  ecuacion={"T_ref,inf = 0.00 °C"}
                  sustitucion={`Referencia activa = ${formAmbiente.tempCongelacionRef.toFixed(2)} °C`}
                  descripcion="El punto de fusión de una mezcla 50/50 de agua pura y hielo picado es 0.00 °C (variación < 0.01 °C con la presión)."
                />
              </label>
              <NumericInput
                value={formAmbiente.tempCongelacionRef}
                onChange={(v) =>
                  setFormAmbiente((p) => ({ ...p, tempCongelacionRef: v }))
                }
                decimals={2}
                min={-10}
                max={10}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>

            <div>
              <label className="mb-1 flex items-center gap-1.5 font-medium">
                <span>Ref. Ebullición (°C)</span>
                <FormulaTooltip
                  titulo="Ecuación de Clausius-Clapeyron"
                  archivoModelo="ModeloAtmosfericoBarometrico.hpp"
                  ecuacion={
                    "1/T(K) = 1/373.15 − (R / ΔH_vap) · ln(P / 1013.25)\nT_ref,sup = T(K) − 273.15"
                  }
                  sustitucion={`Con P = ${formAmbiente.presionAtmosferica} hPa → T_ebull ≈ ${estimarEbullicionDesdePresion(formAmbiente.presionAtmosferica).toFixed(2)} °C`}
                  descripcion="Calcula el punto real de ebullición del agua según la presión atmosférica local (R = 8.31446 J/mol·K, ΔH_vap = 40660 J/mol)."
                />
              </label>
              <NumericInput
                value={formAmbiente.tempEbullicionRef}
                onChange={(v) =>
                  setFormAmbiente((p) => ({ ...p, tempEbullicionRef: v }))
                }
                decimals={2}
                min={70}
                max={110}
                disabled={!esAdmin}
                className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
              />
            </div>
          </div>

          <div className="flex flex-wrap items-center justify-between gap-3">
            <label className="flex items-center gap-2 text-xs">
              <input
                type="checkbox"
                disabled={!esAdmin}
                checked={formAmbiente.activa}
                onChange={(e) =>
                  setFormAmbiente({
                    ...formAmbiente,
                    activa: e.target.checked,
                  })
                }
              />
              Compensación barométrica global activa
            </label>

            {esAdmin && (
              <div className="flex flex-wrap items-center gap-2">
                <button
                  type="button"
                  onClick={aplicarEstimacionDesdeAltura}
                  className="btn btn-outline px-3 py-1.5 text-xs"
                >
                  Calcular Presión y Ebullición desde Altura (
                  {formAmbiente.altura} m)
                </button>
                <button
                  type="button"
                  onClick={aplicarEstimacionDesdePresion}
                  className="btn btn-outline px-3 py-1.5 text-xs"
                >
                  Estimar Ebullición desde Presión (
                  {formAmbiente.presionAtmosferica} hPa)
                </button>
                <button
                  type="submit"
                  className="btn btn-solid btn-info px-4 py-1.5 text-xs font-bold"
                >
                  <Save className="h-4 w-4" />
                  Guardar Parámetros Ambientales
                </button>
              </div>
            )}
          </div>
        </form>
      )}

      {/* PESTAÑA 3: WIFI STA + SOFTAP SEPARADOS (Solo ADMIN) */}
      {tab === "WIFI" && esAdmin && (
        <div className="space-y-4">
          {/* Tarjetas de estado en vivo con botones de refrescar y reconectar STA */}
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-2">
            <div className="rounded-xl border border-border bg-card p-4 text-xs shadow-xs">
              <div className="flex items-start justify-between gap-2">
                <div>
                  <div className="font-semibold text-foreground-secondary">
                    Modo Estación (STA · Router)
                  </div>
                  <div className="mt-1 font-mono text-base font-bold">
                    {wifiInfo?.ssidSta || "Sin configurar"} · IP{" "}
                    {wifiInfo?.ipSta || "0.0.0.0"}
                  </div>
                  <div className="mt-1">
                    Estado:{" "}
                    <strong
                      className={
                        wifiInfo?.conectadoARouter
                          ? "text-success"
                          : "text-warning"
                      }
                    >
                      {wifiInfo?.conectadoARouter
                        ? "Conectado al Router"
                        : "Desconectado"}
                    </strong>
                  </div>
                </div>

                <div className="flex flex-wrap items-center justify-end gap-1.5">
                  <button
                    type="button"
                    disabled={cargando || accionWifi !== null}
                    onClick={() => void cargarWifi(true)}
                    className="btn btn-outline px-2 py-1 text-xs"
                    title="Consultar estado actual de WiFi sin caché"
                  >
                    <RefreshCw
                      className={`h-3.5 w-3.5 ${cargando ? "animate-spin" : ""}`}
                    />
                  </button>
                  <button
                    type="button"
                    disabled={accionWifi !== null || !wifiInfo?.ssidSta}
                    onClick={() => void reconectarWifiSta()}
                    className={`btn px-2.5 py-1 text-xs font-semibold ${
                      wifiInfo?.conectadoARouter
                        ? "btn-outline"
                        : "btn-solid btn-warning"
                    }`}
                    title="Reintentar conexión con el router usando las credenciales guardadas"
                  >
                    {accionWifi === "RECONECTAR" ? (
                      <Loader2 className="h-3.5 w-3.5 animate-spin" />
                    ) : (
                      <RotateCw className="h-3.5 w-3.5" />
                    )}
                    Reconectar STA
                  </button>
                </div>
              </div>
            </div>

            <div className="rounded-xl border border-border bg-card p-4 text-xs shadow-xs">
              <div className="font-semibold text-foreground-secondary">
                Punto de Acceso Local (SoftAP)
              </div>
              <div className="mt-1 font-mono text-base font-bold">
                {wifiInfo?.ssidAp || "ESP32-SecureAP"} · IP{" "}
                {wifiInfo?.ipAp || "192.168.4.1"}
              </div>
              <div className="mt-1">
                Estado:{" "}
                <strong
                  className={
                    wifiInfo?.puntoAccesoActivo
                      ? "text-success"
                      : "text-foreground-secondary"
                  }
                >
                  {wifiInfo?.puntoAccesoActivo ? "Activo" : "Inactivo"}
                </strong>
              </div>
            </div>
          </div>

          {/* Dos formularios independientes para no pisar la otra interfaz */}
          <div className="grid grid-cols-1 gap-4 lg:grid-cols-2">
            {/* Tarjeta 1: Solo WiFi STA (Router) */}
            <form
              onSubmit={guardarWifiSta}
              className="flex flex-col justify-between space-y-4 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
            >
              <div className="space-y-3">
                <div>
                  <h2 className="text-sm font-bold">
                    Red Local / Router (Modo STA)
                  </h2>
                  <p className="text-[11px] text-foreground-secondary">
                    Actualiza solo la conexión al router sin reiniciar el Punto
                    de Acceso (SoftAP). Dejá la contraseña vacía para mantener
                    la guardada.
                  </p>
                </div>

                <div className="space-y-3 text-xs">
                  <div>
                    <label className="mb-1 block font-medium">
                      SSID Red Local (STA)
                    </label>
                    <input
                      type="text"
                      maxLength={31}
                      value={formWifiSta.ssidSta}
                      onChange={(e) =>
                        setFormWifiSta({
                          ...formWifiSta,
                          ssidSta: e.target.value,
                        })
                      }
                      placeholder="Nombre de tu red WiFi"
                      className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
                    />
                  </div>
                  <div>
                    <label className="mb-1 block font-medium">
                      Clave WiFi STA (mín. 8 chars WPA2)
                    </label>
                    <PasswordInput
                      maxLength={63}
                      value={formWifiSta.claveSta}
                      onChange={(e) =>
                        setFormWifiSta({
                          ...formWifiSta,
                          claveSta: e.target.value,
                        })
                      }
                      placeholder="Vacío = conservar clave actual"
                      className="w-full rounded border border-border bg-background px-3 py-1.5"
                    />
                  </div>
                </div>
              </div>

              <div className="flex flex-wrap items-center justify-between gap-2 pt-1">
                <button
                  type="submit"
                  disabled={accionWifi !== null}
                  className="btn btn-solid btn-info px-4 py-2 text-xs font-bold"
                >
                  {accionWifi === "STA" ? (
                    <Loader2 className="h-4 w-4 animate-spin" />
                  ) : (
                    <Save className="h-4 w-4" />
                  )}
                  Guardar y Conectar STA
                </button>

                {wifiInfo?.ssidSta && (
                  <button
                    type="button"
                    disabled={accionWifi !== null}
                    onClick={() => void desvincularWifiSta()}
                    className="btn btn-outline btn-error px-3 py-2 text-xs"
                    title="Borrar credenciales del router y desconectar STA"
                  >
                    {accionWifi === "LIMPIAR_STA" ? (
                      <Loader2 className="h-3.5 w-3.5 animate-spin" />
                    ) : (
                      <Unlink className="h-3.5 w-3.5" />
                    )}
                    Olvidar STA
                  </button>
                )}
              </div>
            </form>

            {/* Tarjeta 2: Solo Punto de Acceso (SoftAP) */}
            <form
              onSubmit={guardarWifiAp}
              className="flex flex-col justify-between space-y-4 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
            >
              <div className="space-y-3">
                <div>
                  <h2 className="text-sm font-bold">
                    Punto de Acceso Directo (SoftAP)
                  </h2>
                  <p className="text-[11px] text-foreground-secondary">
                    Actualiza solo la red propia del ESP32 sin desconectar ni
                    alterar la contraseña del router (STA). Dejá la contraseña
                    vacía para mantener la actual.
                  </p>
                </div>

                <div className="space-y-3 text-xs">
                  <div>
                    <label className="mb-1 block font-medium">
                      SSID Punto de Acceso (SoftAP)
                    </label>
                    <input
                      type="text"
                      maxLength={31}
                      required
                      value={formWifiAp.ssidAp}
                      onChange={(e) =>
                        setFormWifiAp({
                          ...formWifiAp,
                          ssidAp: e.target.value,
                        })
                      }
                      placeholder="ESP32-SecureAP"
                      className="w-full rounded border border-border bg-background px-3 py-1.5 font-mono"
                    />
                  </div>
                  <div>
                    <label className="mb-1 block font-medium">
                      Clave SoftAP (mín. 8 chars WPA2)
                    </label>
                    <PasswordInput
                      maxLength={63}
                      value={formWifiAp.claveAp}
                      onChange={(e) =>
                        setFormWifiAp({
                          ...formWifiAp,
                          claveAp: e.target.value,
                        })
                      }
                      placeholder="Vacío = conservar clave actual"
                      className="w-full rounded border border-border bg-background px-3 py-1.5"
                    />
                  </div>
                </div>
              </div>

              <div>
                <button
                  type="submit"
                  disabled={accionWifi !== null}
                  className="btn btn-solid btn-primary px-4 py-2 text-xs font-bold"
                >
                  {accionWifi === "AP" ? (
                    <Loader2 className="h-4 w-4 animate-spin" />
                  ) : (
                    <Save className="h-4 w-4" />
                  )}
                  Guardar SoftAP
                </button>
              </div>
            </form>
          </div>

          {/* Tarjeta 3: Túnel Remoto Saliente (Reverse TCP Relay a ancho completo) */}
          <form
            onSubmit={guardarRelay}
            className="space-y-4 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
          >
            <div className="flex flex-wrap items-start justify-between gap-2">
              <div className="flex items-center gap-2">
                <Cloud className="h-4 w-4 text-primary" />
                <div>
                  <h2 className="text-sm font-bold">
                    Acceso Remoto Autónomo (Reverse TCP Relay)
                  </h2>
                  <p className="text-[11px] text-foreground-secondary">
                    El ESP32 mantiene un túnel saliente contra tu VPS para
                    cruzar CGNAT sin depender de ninguna PC puente.
                  </p>
                </div>
              </div>

              <div className="flex items-center gap-2 text-xs">
                {relayEstado && (
                  <span
                    className={`rounded-full px-2.5 py-0.5 text-[11px] font-semibold ${
                      relayEstado.conectado
                        ? "bg-alert-success-bg text-alert-success-icon"
                        : relayEstado.habilitado
                          ? "bg-warning/15 text-warning"
                          : "bg-alert-inactive-bg text-alert-inactive-icon"
                    }`}
                  >
                    {LABEL_ESTADO_RELAY[relayEstado.estado] ?? "Desconocido"}
                  </span>
                )}
                <button
                  type="button"
                  disabled={cargando || guardandoRelay}
                  onClick={() => void cargarWifi(true)}
                  className="btn btn-outline px-2 py-1 text-xs"
                  title="Refrescar estado en vivo del túnel"
                >
                  <RefreshCw
                    className={`h-3.5 w-3.5 ${cargando ? "animate-spin" : ""}`}
                  />
                </button>
              </div>
            </div>

            {relayEstado && relayEstado.habilitado && (
              <div className="grid grid-cols-2 gap-2 rounded-lg border border-border bg-background p-2.5 font-mono text-[11px] sm:grid-cols-4">
                <div>
                  <span className="text-foreground-secondary">Uptime: </span>
                  <strong>{relayEstado.uptimeSegundos}s</strong>
                </div>
                <div>
                  <span className="text-foreground-secondary">
                    Clientes remotos:{" "}
                  </span>
                  <strong>{relayEstado.clientesRelayActivos}</strong>
                </div>
                <div>
                  <span className="text-foreground-secondary">
                    Reconexiones:{" "}
                  </span>
                  <strong>{relayEstado.reconexionesTotales}</strong>
                </div>
                <div>
                  <span className="text-foreground-secondary">
                    Diagnóstico:{" "}
                  </span>
                  <strong
                    className={
                      relayEstado.ultimoError === ErrorRelay.NINGUNO
                        ? "text-success"
                        : "text-error"
                    }
                  >
                    {LABEL_ERROR_RELAY[relayEstado.ultimoError] ?? "—"}
                  </strong>
                </div>
              </div>
            )}

            {/* Panel de control del Servidor Relay Local de Prueba */}
            <div className="flex flex-wrap items-center justify-between gap-3 rounded-lg border border-border bg-background p-3 text-xs">
              <div className="space-y-0.5">
                <div className="font-semibold">
                  Túnel de Prueba Local (en esta PC)
                </div>
                <div className="font-mono text-[11px] text-foreground-secondary">
                  {relayLocal?.activo
                    ? `Activo en ${relayLocal.ipLan}:${relayLocal.puertoTunel} · Clientes: ${relayLocal.ipLan}:${relayLocal.puertoClientes} · ESP32: ${relayLocal.espConectado ? "Conectado" : "Esperando..."}`
                    : "Apagado · Encendelo para probar el túnel sin VPS usando tu IP local."}
                </div>
              </div>

              <button
                type="button"
                disabled={cambiandoRelayLocal}
                onClick={() => void alternarRelayLocal()}
                className={`btn px-3 py-1.5 text-xs font-bold ${
                  relayLocal?.activo
                    ? "btn-solid btn-error"
                    : "btn-solid btn-success"
                }`}
              >
                {cambiandoRelayLocal ? (
                  <Loader2 className="h-3.5 w-3.5 animate-spin" />
                ) : (
                  <Power className="h-3.5 w-3.5" />
                )}
                {relayLocal?.activo
                  ? "Apagar Túnel de Prueba"
                  : "Encender Túnel de Prueba"}
              </button>
            </div>

            {/* Inputs apilados verticalmente a ancho completo */}
            <div className="space-y-3 text-xs">
              <div>
                <label className="mb-1 block font-medium">
                  Host / IP del Servidor Relay (VPS o IP LAN local)
                </label>
                <input
                  type="text"
                  maxLength={63}
                  value={formRelay.hostRelay}
                  onChange={(e) =>
                    setFormRelay({ ...formRelay, hostRelay: e.target.value })
                  }
                  placeholder="Ej. 192.168.1.20 o relay.tudominio.com"
                  className="w-full rounded border border-border bg-background px-3 py-2 font-mono"
                />
              </div>

              <div>
                <label className="mb-1 block font-medium">
                  Puerto del Túnel (por defecto 9443)
                </label>
                <NumericInput
                  value={formRelay.puerto}
                  onChange={(v) => setFormRelay((p) => ({ ...p, puerto: v }))}
                  decimals={0}
                  allowNegative={false}
                  min={1}
                  max={65535}
                  className="w-full rounded border border-border bg-background px-3 py-2 font-mono"
                />
              </div>

              <div>
                <label className="mb-1 block font-medium">
                  Node ID (identificador del ESP32, máx. 16 caracteres)
                </label>
                <input
                  type="text"
                  maxLength={16}
                  value={formRelay.nodeId}
                  onChange={(e) =>
                    setFormRelay({ ...formRelay, nodeId: e.target.value })
                  }
                  placeholder="ESP32-UNTDF-0001"
                  className="w-full rounded border border-border bg-background px-3 py-2 font-mono"
                />
              </div>

              <div>
                <label className="mb-1 block font-medium">
                  Secreto Compartido (mín. 16 caracteres)
                </label>
                <PasswordInput
                  maxLength={63}
                  value={formRelay.secreto}
                  onChange={(e) =>
                    setFormRelay({ ...formRelay, secreto: e.target.value })
                  }
                  placeholder={
                    relayCfg?.secreto
                      ? "Vacío = conservar secreto actual"
                      : "Clave secreta compartida con el servidor Relay"
                  }
                  className="w-full rounded border border-border bg-background px-3 py-2"
                />
              </div>
            </div>

            <div className="flex flex-wrap items-center justify-between gap-3">
              <label className="flex items-center gap-2 text-xs">
                <input
                  type="checkbox"
                  checked={formRelay.activa}
                  onChange={(e) =>
                    setFormRelay({ ...formRelay, activa: e.target.checked })
                  }
                />
                Activar túnel saliente 24/7 (requiere WiFi STA conectado)
              </label>

              <button
                type="submit"
                disabled={guardandoRelay}
                className="btn btn-solid btn-primary px-4 py-1.5 text-xs font-bold"
              >
                {guardandoRelay ? (
                  <Loader2 className="h-4 w-4 animate-spin" />
                ) : (
                  <Save className="h-4 w-4" />
                )}
                Guardar Túnel Relay
              </button>
            </div>

            {formRelay.hostRelay.trim() && (
              <div className="text-[11px] text-foreground-secondary">
                Dirección para conectar clientes Electron desde Internet:{" "}
                <code className="font-mono font-bold text-foreground">
                  {formRelay.hostRelay.trim()}:8443
                </code>
              </div>
            )}
          </form>
        </div>
      )}

      {/* PESTAÑA 4: SMTP Y DESTINATARIOS DE CORREO */}
      {tab === "CORREOS" && (
        <div className="space-y-4">
          {esAdmin && (
            <form
              onSubmit={guardarSmtp}
              className="space-y-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
            >
              <div className="flex items-center justify-between">
                <h2 className="text-sm font-bold">
                  Servidor Saliente SMTP (Envío Autónomo desde ESP32)
                </h2>
                {smtpCfg && (
                  <span className="font-mono text-[10px] text-foreground-secondary">
                    Mod.: {smtpCfg.fechaModificacion || "—"}
                  </span>
                )}
              </div>

              <div className="grid grid-cols-1 gap-3 text-xs sm:grid-cols-2 lg:grid-cols-4">
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
                  <NumericInput
                    value={formSmtp.puerto}
                    onChange={(v) => setFormSmtp((p) => ({ ...p, puerto: v }))}
                    decimals={0}
                    allowNegative={false}
                    min={1}
                    max={65535}
                    required
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
                  <label className="mb-1 block">
                    Contraseña / App Password
                  </label>
                  <PasswordInput
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
                    TLS (465)
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

              <div className="space-y-2.5 border-t border-border pt-3">
                <div className="text-[11px] text-foreground-secondary">
                  El ESP32 despacha alertas HTML automáticamente al detectar
                  incidentes (y reintenta fallidos al volver WiFi). Para{" "}
                  <strong>Gmail</strong>: host <code>smtp.gmail.com</code>,
                  puerto <code>465</code>, <strong>TLS</strong> y{" "}
                  <strong>App Password</strong> de 16 caracteres.
                </div>

                <div className="flex flex-wrap items-end gap-2 text-xs">
                  <div>
                    <label className="mb-1 block">
                      Destino de prueba (opcional)
                    </label>
                    <input
                      type="email"
                      maxLength={47}
                      value={destinoPruebaSmtp}
                      onChange={(e) => setDestinoPruebaSmtp(e.target.value)}
                      placeholder="Vacío = 1° destinatario activo"
                      className="w-60 rounded border border-border bg-background px-2.5 py-1.5 font-mono"
                    />
                  </div>
                  <button
                    type="button"
                    disabled={probandoSmtp}
                    onClick={() => void probarEnvioSmtp()}
                    className="btn btn-solid btn-info px-3 py-1.5 text-xs font-semibold"
                  >
                    {probandoSmtp ? (
                      <Loader2 className="h-3.5 w-3.5 animate-spin" />
                    ) : (
                      <Send className="h-3.5 w-3.5" />
                    )}
                    Probar Conexión SMTP
                  </button>
                  <button
                    type="button"
                    disabled={probandoSmtp}
                    onClick={() => void despacharPendientesAhora()}
                    className="btn btn-outline px-3 py-1.5 text-xs"
                    title="Diagnóstico: fuerza un ciclo inmediato sin esperar cooldown"
                  >
                    {probandoSmtp ? (
                      <Loader2 className="h-3.5 w-3.5 animate-spin" />
                    ) : (
                      <Mail className="h-3.5 w-3.5" />
                    )}
                    Forzar Ciclo Ahora (Diagnóstico)
                  </button>
                </div>

                {resultadoPruebaSmtp && (
                  <div
                    className={`flex flex-wrap items-center gap-2 rounded-lg border px-3 py-2 text-xs font-medium ${
                      resultadoPruebaSmtp.exito
                        ? "border-alert-success-border bg-alert-success-bg text-alert-success-icon"
                        : "border-alert-err-border bg-alert-err-bg text-alert-err-icon"
                    }`}
                  >
                    {resultadoPruebaSmtp.exito ? (
                      <CheckCircle2 className="h-4 w-4 shrink-0" />
                    ) : (
                      <XCircle className="h-4 w-4 shrink-0" />
                    )}
                    <span className="font-mono">
                      [{resultadoPruebaSmtp.codigoSmtp || "NET"}]
                    </span>
                    <span>{resultadoPruebaSmtp.detalle}</span>
                    {resultadoPruebaSmtp.eventosActualizados > 0 && (
                      <span className="rounded bg-background/80 px-2 py-0.5 font-mono text-[11px] text-foreground">
                        {resultadoPruebaSmtp.eventosActualizados} aviso(s)
                        actualizados
                      </span>
                    )}
                  </div>
                )}
              </div>
            </form>
          )}

          <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
            <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
              <h2 className="text-sm font-bold">
                Destinatarios de Alertas ({correos.length})
              </h2>
              <div className="flex items-center gap-2 text-xs">
                <button
                  type="button"
                  className="btn btn-outline btn-success px-2.5 py-1"
                  disabled={correos.length === 0}
                  onClick={() => void exportarCorreosExcel()}
                  title="Exportar destinatarios a Excel (.xlsx)"
                >
                  <FileSpreadsheet className="h-3.5 w-3.5" />
                  Excel
                </button>
                <button
                  type="button"
                  className="btn btn-outline px-2.5 py-1"
                  onClick={async () => {
                    await safeRpc(() => client.invalidarCache(), "cache", {
                      silent: true,
                    });
                    void cargarCorreosYSmtp();
                  }}
                >
                  <RefreshCw className="h-3.5 w-3.5" />
                </button>
              </div>
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
                      setFormCorreo({
                        ...formCorreo,
                        apellido: e.target.value,
                      })
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

            <DataTable
              data={correos}
              rowKey={(c) => c.idCorreo}
              infiniteScroll
              maxBodyHeightClass="max-h-[360px]"
              searchPlaceholder="Buscar nombre o email..."
              searchValue={(c) =>
                `${c.nombre} ${c.apellido} ${c.correoElectronico}`
              }
              filters={[
                {
                  key: "filtro",
                  label: "Filtro",
                  getValue: (c) => c.tipoRecepcionMensajes,
                  options: Object.entries(LABEL_TIPO_RECEPCION).map(
                    ([value, label]) => ({ value, label })
                  ),
                },
                {
                  key: "envio",
                  label: "Último Envío",
                  getValue: (c) => c.ultimoEstadoEnvio,
                  options: Object.entries(LABEL_ESTADO_ENVIO).map(
                    ([value, label]) => ({ value, label })
                  ),
                },
                {
                  key: "activo",
                  label: "Estado",
                  getValue: (c) => (c.activa ? "1" : "0"),
                  options: [
                    { value: "1", label: "ACTIVO" },
                    { value: "0", label: "INACTIVO" },
                  ],
                },
              ]}
              columns={[
                {
                  key: "id",
                  header: "ID",
                  sortValue: (c) => c.idCorreo,
                  cellClassName: "font-mono",
                  cell: (c) => `#${c.idCorreo}`,
                },
                {
                  key: "destinatario",
                  header: "Destinatario",
                  sortValue: (c) => `${c.nombre} ${c.apellido}`,
                  cellClassName: "font-medium",
                  cell: (c) => `${c.nombre} ${c.apellido}`,
                },
                {
                  key: "email",
                  header: "Email",
                  sortValue: (c) => c.correoElectronico,
                  cellClassName: "font-mono",
                  cell: (c) => c.correoElectronico,
                },
                {
                  key: "filtro",
                  header: "Filtro",
                  sortValue: (c) => c.tipoRecepcionMensajes,
                  cellClassName: "text-[11px]",
                  cell: (c) =>
                    LABEL_TIPO_RECEPCION[c.tipoRecepcionMensajes] ??
                    c.tipoRecepcionMensajes,
                },
                {
                  key: "envio",
                  header: "Último Envío",
                  sortValue: (c) => c.ultimoEstadoEnvio,
                  cellClassName: "text-[11px]",
                  cell: (c) =>
                    LABEL_ESTADO_ENVIO[c.ultimoEstadoEnvio] ??
                    c.ultimoEstadoEnvio,
                },
                {
                  key: "estado",
                  header: "Estado",
                  sortValue: (c) => c.activa,
                  cell: (c) => (
                    <span
                      className={`rounded-full px-2 py-0.5 text-[10px] font-semibold ${
                        c.activa
                          ? "bg-alert-success-bg text-alert-success-icon"
                          : "bg-alert-inactive-bg text-alert-inactive-icon"
                      }`}
                    >
                      {c.activa ? "ACTIVO" : "INACTIVO"}
                    </span>
                  ),
                },
                ...(esAdmin
                  ? ([
                      {
                        key: "acciones",
                        header: "Acciones",
                        headerClassName: "text-right",
                        cellClassName: "text-right",
                        cell: (c) => (
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
                        ),
                      },
                    ] as DataTableColumn<CorreoItemDTO>[])
                  : []),
              ]}
              emptyMessage="No hay destinatarios configurados."
            />
          </div>
        </div>
      )}

      {/* PESTAÑA 5: SERVIDORES NTP */}
      {tab === "NTP" && (
        <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
          <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
            <h2 className="text-sm font-bold">
              Servidores de Sincronización Horaria NTP ({servidoresNtp.length})
            </h2>
            <div className="flex items-center gap-2 text-xs">
              <button
                type="button"
                className="btn btn-outline btn-success px-2.5 py-1"
                disabled={servidoresNtp.length === 0}
                onClick={() => void exportarNtpExcel()}
                title="Exportar servidores NTP a Excel (.xlsx)"
              >
                <FileSpreadsheet className="h-3.5 w-3.5" />
                Excel
              </button>
              <button
                type="button"
                className="btn btn-outline px-2.5 py-1"
                onClick={async () => {
                  await safeRpc(() => client.invalidarCache(), "cache", {
                    silent: true,
                  });
                  void cargarNtp();
                }}
              >
                <RefreshCw className="h-3.5 w-3.5" />
              </button>
            </div>
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
                <NumericInput
                  value={formNtp.prioridad}
                  onChange={(v) => setFormNtp((p) => ({ ...p, prioridad: v }))}
                  decimals={0}
                  allowNegative={false}
                  min={1}
                  max={10}
                  required
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

          <DataTable
            data={servidoresNtp}
            rowKey={(ntp) => ntp.idServer}
            infiniteScroll
            maxBodyHeightClass="max-h-[360px]"
            filters={[
              {
                key: "activo",
                label: "Estado",
                getValue: (n) => (n.activa ? "1" : "0"),
                options: [
                  { value: "1", label: "ACTIVO" },
                  { value: "0", label: "INACTIVO" },
                ],
              },
            ]}
            columns={[
              {
                key: "id",
                header: "ID",
                sortValue: (n) => n.idServer,
                cellClassName: "font-mono",
                cell: (n) => `#${n.idServer}`,
              },
              {
                key: "server",
                header: "Servidor NTP",
                sortValue: (n) => n.server,
                cellClassName: "font-mono font-bold",
                cell: (n) => n.server,
              },
              {
                key: "prioridad",
                header: "Prioridad",
                sortValue: (n) => n.prioridad,
                cellClassName: "font-mono",
                cell: (n) => n.prioridad,
              },
              {
                key: "estado",
                header: "Estado",
                sortValue: (n) => n.activa,
                cell: (n) => (
                  <span
                    className={`rounded-full px-2 py-0.5 text-[10px] font-semibold ${
                      n.activa
                        ? "bg-alert-success-bg text-alert-success-icon"
                        : "bg-alert-inactive-bg text-alert-inactive-icon"
                    }`}
                  >
                    {n.activa ? "ACTIVO" : "INACTIVO"}
                  </span>
                ),
              },
              ...(esAdmin
                ? ([
                    {
                      key: "acciones",
                      header: "Acciones",
                      headerClassName: "text-right",
                      cellClassName: "text-right",
                      cell: (n) => (
                        <div className="inline-flex items-center gap-1">
                          <button
                            className="btn btn-icon btn-info"
                            onClick={() =>
                              setFormNtp({
                                idServer: n.idServer,
                                server: n.server,
                                prioridad: n.prioridad,
                                activa: n.activa,
                              })
                            }
                            title="Editar"
                          >
                            <Edit2 className="h-3.5 w-3.5" />
                          </button>
                          <button
                            className="btn btn-icon btn-error"
                            onClick={() => void eliminarNtp(n.idServer)}
                            title="Eliminar"
                          >
                            <Trash2 className="h-3.5 w-3.5" />
                          </button>
                        </div>
                      ),
                    },
                  ] as DataTableColumn<ServidorNtpItemDTO>[])
                : []),
            ]}
            emptyMessage="Sin servidores NTP configurados."
          />
        </div>
      )}
    </div>
  );
}