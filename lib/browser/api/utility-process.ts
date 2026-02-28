import { EventEmitter } from 'events';
import * as childProcess from 'child_process';
import type {
  UtilityProcess as UtilityProcessType,
  ForkOptions,
} from '@lynx-js/lynxtron/apis/api/utility-process';

class UtilityProcess extends EventEmitter implements UtilityProcessType {
  private child: childProcess.ChildProcess;

  public get pid(): number | undefined {
    return this.child.pid;
  }

  public get stdout(): import('stream').Readable | null {
    return this.child.stdout;
  }

  public get stderr(): import('stream').Readable | null {
    return this.child.stderr;
  }

  constructor(child: childProcess.ChildProcess) {
    super();
    this.child = child;

    this.child.on('message', (message: any) => {
      this.emit('message', message);
    });

    this.child.on('exit', (code, signal) => {
      this.emit('exit', code, signal);
    });

    this.child.on('error', (err) => {
      this.emit('spawn-failed', err);
    });
  }

  postMessage(message: any, transfer?: any[]) {
    if (this.child) {
      // @ts-ignore: child_process.send transferList option requires @types/node >= 15.13.0
      this.child.send(message, undefined, { transferList: transfer });
    }
  }

  kill() {
    if (this.child) {
      this.child.kill();
    }
    return true;
  }

  static fork(modulePath: string, args?: string[], options?: ForkOptions) {
    const child = childProcess.fork(modulePath, args, {
      ...options,
      serialization: 'advanced',
    } as childProcess.ForkOptions);
    return new UtilityProcess(child);
  }
}

export default {
  fork: UtilityProcess.fork,
};
