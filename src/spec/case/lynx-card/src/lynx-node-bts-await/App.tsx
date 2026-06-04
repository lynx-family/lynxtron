// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useEffect } from '@lynx-js/react';

async function testAsync(message: string) {
  return message;
}

async function test(message: string, NativeModule: any) {
  'background-only';
  console.log(
    'test',
    await testAsync(message),
    await NativeModule.nodejs.exposed.hhhh(message),
    NativeModule.nodejs.exposed.heiheihei(message)
  );
  return message;
}

export default function App() {
  // Don't make the effect callback async; some runtimes treat a returned Promise
  // as a cleanup function which can break listener wiring / teardown.
  useEffect(() => {
    const emitter = lynx.getJSModule('GlobalEventEmitter') as any;
    const handler = (params: any) => {
      const api = (NativeModules as any).nodejs.exposed;
      console.log('get node_event msg ', params);
      // test bts v8 microtask_queue_
      // await test(JSON.stringify(params));
      // test bts node env microtask
      const result = api.get(JSON.stringify(params));
      const bridge = (NativeModules as any).bridge as any;
      bridge.send('nodejs_event', result);
    };

    const subscription = emitter.addListener('node_event', handler, lynx);

    return () => {
      if (subscription && typeof subscription.remove === 'function') {
        subscription.remove();
        return;
      }
      if (typeof emitter.removeListener === 'function') {
        emitter.removeListener('node_event', handler, lynx);
      } else if (typeof emitter.off === 'function') {
        emitter.off('node_event', handler, lynx);
      }
    };
  }, []);

  console.log('add node_event listener', test('test-test', NativeModules));

  return (
    <view
      style={{ height: '100%', width: '100%', backgroundColor: 'red' }}
      className="container"
    >
      <text>lynx-node-bts-await</text>
    </view>
  );
}
