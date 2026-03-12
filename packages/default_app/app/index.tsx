import { root } from '@lynx-js/react';
import './index.css';
import FeatureGrid from './components/FeatureGrid';

export default function WebContainer() {
  return (
    <view clip-radius="true" className="outlineFrame">
      <view className="autoWrapper">
        <view className="innerFrame">
          <view className="introduction">
            <text className="lynxtronTitle">Lynxtron</text>
            <text className="contextWrapper">
              <text className="contextLight">
                Build desktop applications with Lynx.
              </text>
            </text>
            <view className="quickStartWrapper">
              <view className="quickStartBackground" />
              <view className="maskGroup">
                <text className="quickStart">Quick Start</text>
              </view>
            </view>
          </view>
          <FeatureGrid />
        </view>
      </view>
    </view>
  );
}

root.render(<WebContainer />);
