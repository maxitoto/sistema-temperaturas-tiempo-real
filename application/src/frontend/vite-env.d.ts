// src/renderer/env.d.ts
import type { ServiceBridge } from '../shared/types/bridge.types';

declare global {
  interface Window {
    service: ServiceBridge;
  }
}