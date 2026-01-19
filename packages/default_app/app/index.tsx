import { root } from '@lynx-js/react';
import './index.scss';
import FeatureGrid from './components/FeatureGrid';

export default function WebContainer() {
  return (
    <view clip-radius="true" className="outlineFrame">
      <view className="autoWrapper">
        <view className="innerFrame">
          <view className="introduction">
            <text className="lynxtronTitle">Lynxtron</text>
            <text className="contextWrapper">
              <text className="contextBold">Electron</text>
              <text className="contextLight">&nbsp;combines&nbsp;</text>
              <text className="contextBold">Node.js and Chrome</text>
              <text className="contextLight">
                &nbsp;technology ideas, using&nbsp;
              </text>
              <text className="contextBold">Node.js and Lynx</text>
              <text className="contextLight">&nbsp;to build&nbsp;</text>
              <text className="contextBold">desktop applications</text>
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
