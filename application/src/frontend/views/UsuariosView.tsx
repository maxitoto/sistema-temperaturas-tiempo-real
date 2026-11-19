import React, { useCallback, useEffect, useState } from "react";
import {
  Users,
  UserCheck,
  UserPlus,
  KeyRound,
  Unlock,
  Trash2,
  RefreshCw,
  Shield,
  Lock,
  CheckCircle2,
  XCircle,
} from "lucide-react";
import { useAuth } from "../contexts/AuthContext";
import { client, safeRpc } from "../lib/service";
import { formatearEpochSegundos } from "../lib/format";
import { LABEL_ROL } from "../lib/labels";
import {
  EstadisticasUsuariosCliente,
  PerfilUsuarioCliente,
  ResumenUsuarioCliente,
  RolUsuario,
} from "#types/rpc.types";

export function UsuariosView() {
  const { usuario: usuarioActual, rol } = useAuth();
  const esAdmin = rol === RolUsuario.ADMIN;

  // --- Mi Perfil (NO_ADMIN y ADMIN) ---
  const [miPerfil, setMiPerfil] = useState<PerfilUsuarioCliente | null>(null);
  const [claveActual, setClaveActual] = useState("");
  const [claveNueva, setClaveNueva] = useState("");
  const [claveConfirm, setClaveConfirm] = useState("");
  const [cambiandoClave, setCambiandoClave] = useState(false);

  // --- Gestión de Usuarios (Solo ADMIN) ---
  const [stats, setStats] = useState<EstadisticasUsuariosCliente | null>(null);
  const [usuarios, setUsuarios] = useState<ResumenUsuarioCliente[]>([]);
  const [soloActivos, setSoloActivos] = useState(false);

  // Alta nuevo usuario
  const [nuevoUsuario, setNuevoUsuario] = useState("");
  const [nuevaClaveUsuario, setNuevaClaveUsuario] = useState("");
  const [nuevoRolUsuario, setNuevoRolUsuario] = useState<RolUsuario>(
    RolUsuario.NO_ADMIN
  );
  const [creandoUsuario, setCreandoUsuario] = useState(false);

  // Reseteo de contraseña por Admin
  const [usuarioResetTarget, setUsuarioResetTarget] = useState<string | null>(
    null
  );
  const [claveResetAdmin, setClaveResetAdmin] = useState("");

  const cargarMiPerfil = useCallback(async () => {
    if (!usuarioActual) return;
    const perf = await safeRpc(
      () => client.obtenerUsuarioPorNombre(usuarioActual),
      "obtenerUsuarioPorNombre"
    );
    if (perf) setMiPerfil(perf);
  }, [usuarioActual]);

  const cargarAdminUsuarios = useCallback(async () => {
    if (!esAdmin) return;
    const [st, lista] = await Promise.all([
      safeRpc(
        () => client.obtenerEstadisticasUsuarios(),
        "obtenerEstadisticasUsuarios",
        { silent: true }
      ),
      safeRpc(
        () => client.listarUsuarios(32, 0, soloActivos),
        "listarUsuarios"
      ),
    ]);
    if (st) setStats(st);
    if (lista) setUsuarios(lista);
  }, [esAdmin, soloActivos]);

  useEffect(() => {
    void cargarMiPerfil();
  }, [cargarMiPerfil]);

  useEffect(() => {
    void cargarAdminUsuarios();
  }, [cargarAdminUsuarios]);

  const handleCambiarMiClave = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!usuarioActual || !claveNueva) return;
    if (claveNueva !== claveConfirm) {
      window.alert("La nueva contraseña y su confirmación no coinciden.");
      return;
    }
    setCambiandoClave(true);
    const res = await safeRpc(
      () =>
        client.cambiarContrasenaUsuario(usuarioActual, claveActual, claveNueva),
      "cambiarContrasenaUsuario",
      { mensajeExito: "Contraseña actualizada correctamente." }
    );
    setCambiandoClave(false);
    if (res) {
      setMiPerfil(res);
      setClaveActual("");
      setClaveNueva("");
      setClaveConfirm("");
    }
  };

  const handleCrearUsuario = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!nuevoUsuario.trim() || !nuevaClaveUsuario) return;
    setCreandoUsuario(true);
    const creado = await safeRpc(
      () =>
        client.crearUsuario(
          nuevoUsuario.trim(),
          nuevaClaveUsuario,
          nuevoRolUsuario
        ),
      "crearUsuario",
      { mensajeExito: `Usuario "${nuevoUsuario.trim()}" creado.` }
    );
    setCreandoUsuario(false);
    if (creado) {
      setNuevoUsuario("");
      setNuevaClaveUsuario("");
      setNuevoRolUsuario(RolUsuario.NO_ADMIN);
      void cargarAdminUsuarios();
    }
  };

  const handleActualizarRolYEstado = async (
    user: ResumenUsuarioCliente,
    nuevoRol: number,
    activa: boolean
  ) => {
    const act = await safeRpc(
      () => client.actualizarRolYEstadoUsuario(user.usuario, nuevoRol, activa),
      "actualizarRolYEstadoUsuario",
      { mensajeExito: `Permisos de "${user.usuario}" actualizados.` }
    );
    if (act) {
      void cargarAdminUsuarios();
      if (user.usuario === usuarioActual) void cargarMiPerfil();
    }
  };

  const handleDesbloquear = async (nombreUsuario: string) => {
    const res = await safeRpc(
      () => client.desbloquearUsuario(nombreUsuario),
      "desbloquearUsuario",
      { mensajeExito: `Cuenta "${nombreUsuario}" desbloqueada.` }
    );
    if (res) void cargarAdminUsuarios();
  };

  const handleResetAdmin = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!usuarioResetTarget || !claveResetAdmin) return;
    const res = await safeRpc(
      () =>
        client.cambiarContrasenaUsuario(
          usuarioResetTarget,
          "",
          claveResetAdmin
        ),
      "cambiarContrasenaUsuario",
      {
        mensajeExito: `Contraseña de "${usuarioResetTarget}" restablecida por Administrador.`,
      }
    );
    if (res) {
      setUsuarioResetTarget(null);
      setClaveResetAdmin("");
    }
  };

  const handleEliminarUsuario = async (nombreUsuario: string) => {
    if (nombreUsuario.toLowerCase() === "admin") return;
    if (
      !window.confirm(
        `¿Eliminar permanentemente al usuario "${nombreUsuario}" del SGBD?`
      )
    ) {
      return;
    }
    await safeRpc(
      () => client.eliminarUsuario(nombreUsuario),
      "eliminarUsuario",
      { mensajeExito: `Usuario "${nombreUsuario}" eliminado.` }
    );
    void cargarAdminUsuarios();
  };

  return (
    <div className="space-y-4 p-4">
      {/* Encabezado */}
      <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
        <div className="flex items-center gap-3">
          <div className="flex h-10 w-10 items-center justify-center rounded-lg bg-primary-light text-primary">
            <Users className="h-5 w-5" />
          </div>
          <div>
            <h1 className="text-lg font-bold">
              Control de Acceso RBAC y Gestión de Usuarios
            </h1>
            <p className="text-xs text-foreground-secondary">
              Perfil de sesión L2 y administración de cuentas en tabla{" "}
              <code>Usuario</code>
            </p>
          </div>
        </div>
        <button
          type="button"
          className="btn btn-outline px-3 py-1.5 text-xs"
          onClick={() => {
            void cargarMiPerfil();
            if (esAdmin) void cargarAdminUsuarios();
          }}
        >
          <RefreshCw className="h-3.5 w-3.5" />
          Refrescar
        </button>
      </div>

      {/* Sección 1: Mi Perfil y Cambio de Contraseña Propia */}
      <div className="grid grid-cols-1 gap-4 lg:grid-cols-2">
        <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
          <div className="mb-3 flex items-center gap-2 text-sm font-bold">
            <UserCheck className="h-4 w-4 text-primary" />
            Mi Perfil Autenticado
          </div>
          {miPerfil ? (
            <div className="grid grid-cols-2 gap-3 text-xs">
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">Usuario</div>
                <div className="mt-0.5 font-mono text-sm font-bold">
                  {miPerfil.usuario} (ID #{miPerfil.idUsuario})
                </div>
              </div>
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">Rol Asignado</div>
                <div className="mt-0.5 font-semibold text-primary">
                  {LABEL_ROL[miPerfil.rol] ?? `Rol ${miPerfil.rol}`}
                </div>
              </div>
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">Estado Cuenta</div>
                <div className="mt-0.5 font-semibold">
                  {miPerfil.activa ? "Activa" : "Inactiva"} ·{" "}
                  {miPerfil.bloqueado
                    ? "Bloqueada"
                    : `${miPerfil.intentosFallidos} intentos fallidos`}
                </div>
              </div>
              <div className="rounded-lg border border-border bg-background p-2.5">
                <div className="text-foreground-secondary">Auditoría Temporal</div>
                <div className="mt-0.5 font-mono text-[11px]">
                  Creado: {miPerfil.creadoEl || "—"}
                  <br />
                  Mod.: {miPerfil.actualizadoEl || "—"}
                </div>
              </div>
            </div>
          ) : (
            <p className="text-xs text-foreground-secondary">
              Cargando perfil desde el microcontrolador...
            </p>
          )}
        </div>

        <form
          onSubmit={handleCambiarMiClave}
          className="space-y-3 rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs"
        >
          <div className="flex items-center gap-2 text-sm font-bold">
            <KeyRound className="h-4 w-4 text-warning" />
            Cambiar Mi Contraseña
          </div>
          <div className="grid grid-cols-1 gap-2 sm:grid-cols-3 text-xs">
            <div>
              <label className="mb-1 block">Clave Actual</label>
              <input
                type="password"
                maxLength={27}
                value={claveActual}
                onChange={(e) => setClaveActual(e.target.value)}
                required={!esAdmin}
                placeholder={esAdmin ? "(Opcional Admin)" : "••••••••"}
                className="w-full rounded border border-border bg-background px-2.5 py-1.5"
              />
            </div>
            <div>
              <label className="mb-1 block">Nueva Contraseña</label>
              <input
                type="password"
                maxLength={27}
                value={claveNueva}
                onChange={(e) => setClaveNueva(e.target.value)}
                required
                placeholder="••••••••"
                className="w-full rounded border border-border bg-background px-2.5 py-1.5"
              />
            </div>
            <div>
              <label className="mb-1 block">Confirmar Nueva</label>
              <input
                type="password"
                maxLength={27}
                value={claveConfirm}
                onChange={(e) => setClaveConfirm(e.target.value)}
                required
                placeholder="••••••••"
                className="w-full rounded border border-border bg-background px-2.5 py-1.5"
              />
            </div>
          </div>
          <button
            type="submit"
            disabled={cambiandoClave || !claveNueva}
            className="btn btn-solid btn-warning px-4 py-1.5 text-xs font-semibold"
          >
            <Lock className="h-3.5 w-3.5" />
            Actualizar Mi Contraseña
          </button>
        </form>
      </div>

      {/* Sección 2: Administración Completa de Usuarios (Solo ADMIN) */}
      {esAdmin && (
        <div className="space-y-4">
          {stats && (
            <div className="grid grid-cols-2 gap-3 sm:grid-cols-4">
              <div className="rounded-xl border border-border bg-card p-3.5 shadow-xs">
                <div className="text-xs text-foreground-secondary">
                  Total Usuarios
                </div>
                <div className="mt-1 text-xl font-bold">
                  {stats.totalUsuarios}
                </div>
              </div>
              <div className="rounded-xl border border-border bg-card p-3.5 shadow-xs">
                <div className="text-xs text-foreground-secondary">Activos</div>
                <div className="mt-1 text-xl font-bold text-success">
                  {stats.usuariosActivos}
                </div>
              </div>
              <div className="rounded-xl border border-border bg-card p-3.5 shadow-xs">
                <div className="text-xs text-foreground-secondary">
                  Bloqueados
                </div>
                <div className="mt-1 text-xl font-bold text-error">
                  {stats.usuariosBloqueados}
                </div>
              </div>
              <div className="rounded-xl border border-border bg-card p-3.5 shadow-xs">
                <div className="text-xs text-foreground-secondary">
                  Límite Intentos Fallidos
                </div>
                <div className="mt-1 text-xl font-bold text-warning">
                  {stats.maxIntentosFallidos}
                </div>
              </div>
            </div>
          )}

          {/* Formulario Crear Nuevo Usuario */}
          <form
            onSubmit={handleCrearUsuario}
            className="flex flex-wrap items-end gap-3 rounded-xl border border-border bg-card p-4 text-xs text-card-foreground shadow-xs"
          >
            <div className="flex items-center gap-2 self-center font-bold text-sm mr-2">
              <UserPlus className="h-4 w-4 text-primary" />
              Alta de Usuario:
            </div>
            <div>
              <label className="mb-1 block">Nombre de Usuario (máx. 19)</label>
              <input
                type="text"
                maxLength={19}
                value={nuevoUsuario}
                onChange={(e) => setNuevoUsuario(e.target.value)}
                placeholder="operador_lab"
                required
                className="rounded border border-border bg-background px-2.5 py-1.5 font-mono"
              />
            </div>
            <div>
              <label className="mb-1 block">Contraseña Inicial</label>
              <input
                type="password"
                maxLength={27}
                value={nuevaClaveUsuario}
                onChange={(e) => setNuevaClaveUsuario(e.target.value)}
                placeholder="••••••••"
                required
                className="rounded border border-border bg-background px-2.5 py-1.5"
              />
            </div>
            <div>
              <label className="mb-1 block">Rol Inicial RBAC</label>
              <select
                value={nuevoRolUsuario}
                onChange={(e) =>
                  setNuevoRolUsuario(Number(e.target.value) as RolUsuario)
                }
                className="rounded border border-border bg-background px-2.5 py-1.5"
              >
                <option value={RolUsuario.NINGUNO}>0 · NINGUNO</option>
                <option value={RolUsuario.NO_ADMIN}>
                  1 · NO_ADMIN (Operador)
                </option>
                <option value={RolUsuario.ADMIN}>2 · ADMIN</option>
              </select>
            </div>
            <button
              type="submit"
              disabled={creandoUsuario}
              className="btn btn-solid btn-primary px-4 py-1.5 text-xs font-semibold"
            >
              <UserPlus className="h-3.5 w-3.5" />
              Crear Usuario
            </button>
          </form>

          {/* Barra inline para Reset de Contraseña por Admin */}
          {usuarioResetTarget && (
            <form
              onSubmit={handleResetAdmin}
              className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-alert-warn-border bg-alert-warn-bg p-3 text-xs"
            >
              <div className="flex items-center gap-2 font-semibold">
                <Shield className="h-4 w-4 text-warning" />
                <span>
                  Restablecer contraseña de{" "}
                  <code className="font-mono">{usuarioResetTarget}</code> (sin
                  clave previa):
                </span>
              </div>
              <div className="flex items-center gap-2">
                <input
                  type="password"
                  maxLength={27}
                  value={claveResetAdmin}
                  onChange={(e) => setClaveResetAdmin(e.target.value)}
                  placeholder="Nueva contraseña..."
                  required
                  className="rounded border border-border bg-background px-2.5 py-1"
                />
                <button
                  type="submit"
                  className="btn btn-solid btn-warning px-3 py-1 text-xs font-semibold"
                >
                  Guardar Clave
                </button>
                <button
                  type="button"
                  onClick={() => {
                    setUsuarioResetTarget(null);
                    setClaveResetAdmin("");
                  }}
                  className="btn btn-outline px-2.5 py-1 text-xs"
                >
                  Cancelar
                </button>
              </div>
            </form>
          )}

          {/* Tabla de Usuarios */}
          <div className="rounded-xl border border-border bg-card p-4 text-card-foreground shadow-xs">
            <div className="mb-3 flex flex-wrap items-center justify-between gap-2">
              <h2 className="text-sm font-bold">
                Cuentas Registradas ({usuarios.length})
              </h2>
              <label className="flex cursor-pointer items-center gap-1.5 text-xs">
                <input
                  type="checkbox"
                  checked={soloActivos}
                  onChange={(e) => setSoloActivos(e.target.checked)}
                />
                Mostrar solo activos
              </label>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-left text-xs">
                <thead className="border-b border-border bg-secondary/60 text-foreground-secondary uppercase">
                  <tr>
                    <th className="px-3 py-2">ID</th>
                    <th className="px-3 py-2">Usuario</th>
                    <th className="px-3 py-2">Rol RBAC</th>
                    <th className="px-3 py-2">Estado</th>
                    <th className="px-3 py-2">Seguridad / Intentos</th>
                    <th className="px-3 py-2">Última Actualización</th>
                    <th className="px-3 py-2 text-right">Acciones</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-border">
                  {usuarios.map((u) => {
                    const esSuperAdmin = u.usuario.toLowerCase() === "admin";
                    return (
                      <tr key={u.idUsuario} className="hover:bg-secondary/40">
                        <td className="px-3 py-2 font-mono">#{u.idUsuario}</td>
                        <td className="px-3 py-2 font-mono font-bold">
                          {u.usuario}
                        </td>
                        <td className="px-3 py-2">
                          <select
                            value={u.rol}
                            disabled={esSuperAdmin}
                            onChange={(e) =>
                              void handleActualizarRolYEstado(
                                u,
                                Number(e.target.value),
                                u.activa
                              )
                            }
                            className="rounded border border-border bg-background px-2 py-1 text-xs"
                          >
                            <option value={RolUsuario.NINGUNO}>NINGUNO</option>
                            <option value={RolUsuario.NO_ADMIN}>NO_ADMIN</option>
                            <option value={RolUsuario.ADMIN}>ADMIN</option>
                          </select>
                        </td>
                        <td className="px-3 py-2">
                          <button
                            type="button"
                            disabled={esSuperAdmin}
                            onClick={() =>
                              void handleActualizarRolYEstado(
                                u,
                                u.rol,
                                !u.activa
                              )
                            }
                            className={`inline-flex items-center gap-1 rounded-full px-2.5 py-0.5 text-[10px] font-semibold ${
                              u.activa
                                ? "bg-alert-success-bg text-alert-success-icon"
                                : "bg-alert-inactive-bg text-alert-inactive-icon"
                            }`}
                          >
                            {u.activa ? (
                              <CheckCircle2 className="h-3 w-3" />
                            ) : (
                              <XCircle className="h-3 w-3" />
                            )}
                            {u.activa ? "ACTIVO" : "INACTIVO"}
                          </button>
                        </td>
                        <td className="px-3 py-2">
                          {u.bloqueado ? (
                            <span className="rounded bg-alert-err-bg px-2 py-0.5 text-[10px] font-bold text-alert-err-icon">
                              BLOQUEADO ({u.intentosFallidos} fallos)
                            </span>
                          ) : (
                            <span className="text-foreground-secondary">
                              OK ({u.intentosFallidos} fallos)
                            </span>
                          )}
                        </td>
                        <td className="px-3 py-2 font-mono text-foreground-secondary">
                          {formatearEpochSegundos(u.actualizadoEpoch)}
                        </td>
                        <td className="px-3 py-2 text-right">
                          <div className="inline-flex items-center gap-1">
                            {u.bloqueado && (
                              <button
                                className="btn btn-icon btn-success"
                                onClick={() => void handleDesbloquear(u.usuario)}
                                title="Desbloquear cuenta"
                              >
                                <Unlock className="h-3.5 w-3.5" />
                              </button>
                            )}
                            <button
                              className="btn btn-icon btn-warning"
                              onClick={() => {
                                setUsuarioResetTarget(u.usuario);
                                setClaveResetAdmin("");
                              }}
                              title="Resetear contraseña como Admin"
                            >
                              <KeyRound className="h-3.5 w-3.5" />
                            </button>
                            <button
                              className="btn btn-icon btn-error"
                              disabled={esSuperAdmin}
                              onClick={() =>
                                void handleEliminarUsuario(u.usuario)
                              }
                              title={
                                esSuperAdmin
                                  ? "La cuenta raíz admin no puede eliminarse"
                                  : "Eliminar usuario"
                              }
                            >
                              <Trash2 className="h-3.5 w-3.5" />
                            </button>
                          </div>
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}