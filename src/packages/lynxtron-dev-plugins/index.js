import { execSync } from 'child_process';
import spawn from 'cross-spawn';

let lynxtronProcess = null;
let queue = Promise.resolve(null);
const isWin = process.platform === 'win32';

function debounce(func, wait) {
  let timeout;
  return function (...args) {
    clearTimeout(timeout);
    timeout = setTimeout(() => func.apply(this, args), wait);
  };
}

const restartLynxtron = debounce((options) => {
  queue = queue.then(() => {
    try {
      if (lynxtronProcess && lynxtronProcess.pid) {
        const pid = lynxtronProcess.pid;
        if (isWin) {
          execSync(`taskkill /pid ${pid} /f /t`);
        } else {
          try {
            process.kill(-pid, 'SIGKILL');
          } catch {}
        }
      }
    } catch {}

    const { entry, args = [], env = {}, command = 'lynxtron' } = options;
    // The Lynxtron runtime treats argv[1] as the application directory.
    // Keep it first so runtime flags cannot be mistaken for the app path by
    // the runtime or generated AutoLink loaders.
    const spawnArgs = [entry, ...args];

    lynxtronProcess = spawn(command, spawnArgs, {
      stdio: 'inherit',
      env: {
        ...process.env,
        ...env,
      },
      detached: true,
    });

    return lynxtronProcess;
  });
}, 300);

export function pluginLynxtron(options = {}) {
  const { isDev, entry, args = [], env, command } = options;
  return {
    name: 'lynxtron-plugin',
    apply(compiler) {
      if (!compiler.options.optimization) {
        compiler.options.optimization = {};
      }
      compiler.options.optimization.minimize = !isDev;
      compiler.options.optimization.nodeEnv = false;

      compiler.hooks.done.tap('LynxtronStart', () => {
        if (!entry) {
          return;
        }
        const extraArgs = [...args];
        restartLynxtron({
          entry,
          args: extraArgs,
          env,
          command,
        });
      });
    },
  };
}
