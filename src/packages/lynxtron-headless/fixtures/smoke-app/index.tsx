import { root, useState } from '@lynx-js/react';
import './index.css';

function App() {
  const [count, setCount] = useState(0);
  const active = count > 0;

  return (
    <view className="page">
      <view
        className={`panel ${active ? 'panelActive' : ''}`}
        bindtap={() => setCount((value) => value + 1)}
      >
        <text className={`label ${active ? 'labelActive' : ''}`}>
          {`tap ${count}`}
        </text>
      </view>
    </view>
  );
}

root.render(<App />);
