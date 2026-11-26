// src/renderer/env.d.ts
import type { ServiceBridge } from '../shared/types/bridge.types';

declare module 'src/frontend/styles/index.css';

declare global {
  interface Window {
    service: ServiceBridge;
  }
}