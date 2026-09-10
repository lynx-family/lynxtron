import './App.css';
export function App() {
  return <view className="screen"><text className="heading">LYNX HOST — FRESH PUBLISHED ALPHA</text><text className="subtitle">CEF below must show a green page with white title and a white version panel.</text><webview src="http://127.0.0.1:18769/" style={{ width: '920px', height: '560px' }} /></view>;
}
