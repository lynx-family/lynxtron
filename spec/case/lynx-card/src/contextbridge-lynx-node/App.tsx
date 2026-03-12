import { useEffect } from '@lynx-js/react';

export default function App() {
  useEffect(() => {
    const emitter = lynx.getJSModule('GlobalEventEmitter') as any;
    const handler = (params: any) => {
      const api = (NativeModules as any).nodejs.getExposed();
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

  return (
    <view style={{ flexDirection: 'column' as const }} className="container">
      <text>contextbrige-lynx-node</text>
    </view>
  );
}
