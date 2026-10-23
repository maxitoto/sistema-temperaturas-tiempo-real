```markdown
# ⚡ Modern Electron + React Architecture

Una arquitectura robusta, segura y escalable para aplicaciones de escritorio. Construida con el ecosistema moderno de Node.js, este proyecto implementa una separación estricta de responsabilidades entre el proceso principal (Backend) y el renderizador (Frontend), garantizando la máxima seguridad con `contextIsolation` y un sistema de IPC automatizado.

## 🛠 Stack Tecnológico

- **Frontend:** React 19 + Vite 8 + TypeScript
- **Backend:** Electron 43 + Node 22
- **Gestor de paquetes:** pnpm

## 🏗 Arquitectura del Proyecto

El código fuente está dividido en tres dominios principales dentro de la carpeta `src/` para mantener una escalabilidad impecable:

```text
src/
├── backend/       # Proceso principal de Electron (Node.js)
│   ├── main.ts    # Punto de entrada, configuración de ventanas y seguridad
│   ├── handles.ts # Auto-registro dinámico de canales IPC
│   ├── preload.ts # Puente seguro (contextBridge) hacia el frontend
│   └── electron-env.d.ts # Definiciones de tipos para el ecosistema Electron
│
├── frontend/      # Proceso de renderizado (React UI)
│   ├── main.tsx   # Punto de entrada de React
│   ├── App.tsx    # Componente principal
│   ├── components/# Componentes reutilizables de UI
│   ├── hooks/     # Custom hooks de React
│   ├── tabs/      # Vistas o pestañas de la aplicación
│   └── styles/    # Hojas de estilo modulares
│
└── shared/        # Única Fuente de Verdad (Single Source of Truth)
    └── services.ts# Definición centralizada de APIs (prod/dev)

```

## 🔒 Seguridad y Comunicación (IPC)

Este proyecto no utiliza llamadas IPC manuales propensas a errores. En su lugar, implementa un **Auto-Registro de Servicios**:

1. **Definición Centralizada:** Todas las funciones del backend se definen en `src/shared/services.ts` (divididas en `prod` y `dev`).
2. **Auto-Registro Backend:** `handles.ts` itera sobre los servicios y crea automáticamente los `ipcMain.handle`.
3. **Puente Tipado:** `preload.ts` genera las funciones invocables dinámicamente.
4. **Tipado Estricto:** El frontend consume estas APIs a través de `window.electronAPI.prod` con autocompletado total gracias a TypeScript.

*Seguridad habilitada:* `contextIsolation: true`, `nodeIntegration: false`, `sandbox: true`.

## 🚀 Comandos de Desarrollo

Asegúrate de tener instalado [pnpm](https://pnpm.io/).

### Instalación

```bash
pnpm install
```

### Entorno de Desarrollo

Inicia Vite con Hot-Module-Replacement (HMR) y el proceso de Electron simultáneamente.

```bash
pnpm run dev
```

### Empaquetado para Producción

Compila el frontend y el backend, y genera el ejecutable final usando `electron-builder` (basado en la configuración de `electron-builder.json5`).

```bash
pnpm run build
```