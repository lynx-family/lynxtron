export interface MainThreadBridge {
  [methodName: string]: (data: any) => Promise<any> | any;
}

export interface BackgroundServiceConfig {
  workerURL: string;
  methods: string[];
}

export interface HostConfig {
  bridge?: MainThreadBridge;
  nodejs?: BackgroundServiceConfig;
}

/**
 * Setup Symmetric Host for Web.
 * @param lynxView The <lynx-view> element.
 * @param config Configuration for bridge and nodejs.
 */
export function setupSymmetricHost(lynxView: any, config?: HostConfig): void;
