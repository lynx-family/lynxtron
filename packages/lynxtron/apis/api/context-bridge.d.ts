export interface ContextBridge {
  exposeInLynxBTS(apis: Record<string, any>): void;
}
export const contextBridge: ContextBridge;
