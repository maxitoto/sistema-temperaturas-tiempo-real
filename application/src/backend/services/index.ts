// src/backend/services/index.ts
import { clientServices }    from './clientServices';
import { developerServices } from './developerServices';

export const serviceRegistry = {
  client:    clientServices,
  developer: developerServices,
} as const;

export type ServiceRegistry = typeof serviceRegistry;
export type Namespace       = keyof ServiceRegistry;