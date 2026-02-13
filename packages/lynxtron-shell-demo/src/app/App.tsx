import { useCallback } from '@lynx-js/react';
import './App.css';
import placeholder from '@assets/placeholder.png';

export function App() {
  const handleTap = useCallback(() => {
    console.log('[App] handleTap triggered', NativeModules.nodejs.getExposed());
    NativeModules.bridge.call(
      'showDialog',
      {
        message: NativeModules.nodejs.getExposed().echo('Hello from Lynxtron!'),
      },
      () => {
        console.log('[App] bridge.request callback fired');
      }
    );
  }, []);

  return (
    <view className="Background">
      <image className="BackgroundImage" src={placeholder}></image>
      <view className="Container" bindtap={handleTap}>
        <text className="Title">Hello, Lynxtron</text>
        <text className="Hint">Tap card to show native dialog</text>
      </view>
    </view>
  );
}
