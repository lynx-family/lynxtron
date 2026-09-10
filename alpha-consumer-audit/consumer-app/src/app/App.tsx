import { useEffect, useState } from '@lynx-js/react';
import '@alpha-consumer/widget';

function audit(message: string) {
  'background only';
  console.log('[ALPHA_AUDIT]', message);
  NativeModules.bridge.call('audit', { message }, () => {});
}

export function App() {
  const [loaded, setLoaded] = useState(false);
  useEffect(() => {
    audit('UI_MOUNTED');
    lynx.createSelectorQuery().select('#consumer-widget').invoke({
      method: 'setValue',
      params: { value: 'ALPHA_WIDGET_100' },
      success: () => audit('WIDGET_INVOKE_OK'),
      fail: (error: unknown) => audit(`WIDGET_INVOKE_FAILED ${JSON.stringify(error)}`),
    }).exec();
  }, []);
  return (
    <view style={{ width: '100%', height: '100%', padding: '24px', display: 'flex', flexDirection: 'column', backgroundColor: '#eeeeee' }}>
      <text style={{ fontSize: '24px', color: '#111111' }}>Alpha consumer release 100</text>
      <text>{loaded ? 'CEF_LOAD_OK' : 'CEF loading'}</text>
      <x-consumer-widget id="consumer-widget" value="ALPHA_WIDGET_100" style={{ width: '100%', height: '90px' }} />
      <webview
        src="http://127.0.0.1:17981/"
        style={{ width: '100%', height: '340px' }}
        bindload={() => { setLoaded(true); audit('CEF_LOAD_OK'); }}
        binderror={(error: unknown) => audit(`CEF_LOAD_FAILED ${JSON.stringify(error)}`)}
      />
    </view>
  );
}
