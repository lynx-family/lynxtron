import { root, useState } from '@lynx-js/react';
import './index.css';

const rows = [
  ['Revenue', '$18.2k', '+12%'],
  ['Latency', '42ms', '-8%'],
  ['Errors', '3', 'stable'],
];

function App() {
  const [selected, setSelected] = useState(false);
  const plan = selected ? 'Pro' : 'Basic';
  const total = selected ? '$57' : '$42';
  const status = selected ? 'selected' : 'idle';
  const items = selected ? 4 : 3;

  return (
    <view className="page">
      <view className="header">
        <text className="title">Headless Commerce Console</text>
        <text className="subtitle">Status {status}</text>
      </view>

      <view className="summary">
        <view className="summaryBlock">
          <text className="caption">Selected {plan}</text>
          <text className="metric">Cart total {total}</text>
        </view>
        <view className="summaryBlock right">
          <text className="caption">Items {items}</text>
          <text className="metric">Checkout ready</text>
        </view>
      </view>

      <view className="grid">
        {rows.map(([label, value, trend]) => (
          <view className="row" key={label}>
            <text className="rowLabel">{label}</text>
            <text className="rowValue">{value}</text>
            <text className="rowTrend">{trend}</text>
          </view>
        ))}
      </view>

      <view
        className={`action ${selected ? 'actionSelected' : ''}`}
        bindtap={() => setSelected(true)}
      >
        <view className="actionText">
          <text className="actionTitle">Upgrade plan</text>
          <text className="actionBody">
            {selected ? 'Selected Pro' : 'Selected Basic'}
          </text>
        </view>
        <text className="actionPrice">{selected ? '$57' : '$42'}</text>
      </view>
    </view>
  );
}

root.render(<App />);
