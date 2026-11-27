import { app, BrowserWindow } from 'electron';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { startHandlers } from './handles';

const __dirname = dirname(fileURLToPath(import.meta.url));
const APP_ROOT = app.isPackaged
  ? app.getAppPath() // Siempre devuelve ".../resources/app.asar"
  : join(__dirname, '../../');

process.env.APP_ROOT = APP_ROOT;

export const VITE_DEV_SERVER_URL = process.env['VITE_DEV_SERVER_URL'];
export const MAIN_DIST = __dirname;
export const RENDERER_DIST = join(APP_ROOT, 'dist');

if (VITE_DEV_SERVER_URL) {
  process.env['ELECTRON_DISABLE_SECURITY_WARNINGS'] = 'true';
}

process.env.VITE_PUBLIC = VITE_DEV_SERVER_URL
  ? join(APP_ROOT, 'public')
  : RENDERER_DIST;

let mainWindow: BrowserWindow | null = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    icon: join(process.env.VITE_PUBLIC!, 'electron-vite.svg'),
    webPreferences: {
      preload: join(__dirname, 'preload.mjs'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });

  if (VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(VITE_DEV_SERVER_URL);
  } else {
    mainWindow.loadFile(join(RENDERER_DIST, 'index.html'));
  }
}

app.disableHardwareAcceleration();

app.whenReady().then(async () => {
  console.log('APP_ROOT:', APP_ROOT);
  console.log('RENDERER_DIST:', RENDERER_DIST);
  console.log('isPackaged:', app.isPackaged);
  startHandlers();
  createWindow();
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow();
  }
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
    mainWindow = null;
  }
});