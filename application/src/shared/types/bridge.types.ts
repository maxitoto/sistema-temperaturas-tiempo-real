import type { ServiceRegistry } from '../../backend/services';

type BridgeMethod<F> =
  F extends (...args: infer A) => infer R
      ? (...args: A) => R          // async: se devuelve tal cual
    : never;

export type ServiceBridge = {
  [N in keyof ServiceRegistry]: {
    [M in keyof ServiceRegistry[N]]: BridgeMethod<ServiceRegistry[N][M]>;
  };
};