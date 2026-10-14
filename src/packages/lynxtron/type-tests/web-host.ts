import { setupSymmetricHost } from '../apis/web-host.js';
import type { BackgroundServiceConfig, HostConfig } from '../apis/web-host.js';

const nodejs: BackgroundServiceConfig = {
  scriptURL: './nodejs-adapter-web.js',
};

const config: HostConfig = {
  bridge: {
    call(method: string, params: { message: string }) {
      return { method, message: params.message };
    },
    async showDialog(data: { message: string }) {
      return { shown: Boolean(data.message) };
    },
  },
  nodejs,
};

setupSymmetricHost(document.createElement('lynx-view'), config);
setupSymmetricHost(document.createElement('lynx-view'));

const obsolete: BackgroundServiceConfig = {
  // @ts-expect-error The implementation reads scriptURL, not workerURL.
  workerURL: './worker.js',
  methods: ['echo'],
};

// @ts-expect-error A configured background adapter requires a script URL.
const missingURL: BackgroundServiceConfig = {};

void obsolete;
void missingURL;
