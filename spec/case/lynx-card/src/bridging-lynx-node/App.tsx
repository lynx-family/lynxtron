import { useEffect } from '@lynx-js/react';

export default function App() {
  useEffect(() => {
    const bridge = (NativeModules as any).bridge as any;
    bridge.call('onRender-test-event', { msg: 'test-test' }, (params: any) => {
      bridge.send('callback', {
        from: '-lynx-invoke-callback',
        rawParams: params,
      });
    });
  }, []);

  return (
    <view style={{ flexDirection: 'column' as const }} className="container">
      <text>Hello, ReactLynx 3!</text>
    </view>
  );
}
