import { FunctionComponent } from '@lynx-js/react';

import LynxTronIcon from './icons/lynxtron.png?inline';
import FrameIcon from './icons/frame.png?inline';

export interface HomePageProps {
  onConvertToLynxTab?: () => void;
}

export const HomePage: FunctionComponent<HomePageProps> = (props) => {
  return (
    <view className="homepage">
      <view className="brand-row">
        <image src={LynxTronIcon} className="brand-icon" />
        <text className="brand-text">Lynxtron</text>
      </view>

      <view className="quick-access-grid">
        <text className="quick-access-title">Quick Access</text>
        <view
          className="quick-card"
          bindtap={() => props.onConvertToLynxTab?.()}
        >
          <image clip-radius="true" src={FrameIcon} className="icon16" />
          <text className="quick-card-title">lynxjs</text>
        </view>
        <view
          className="quick-card"
          bindtap={() => props.onConvertToLynxTab?.()}
        >
          <image clip-radius="true" src={FrameIcon} className="icon16" />
          <text className="quick-card-title">lynxjs</text>
        </view>
      </view>
    </view>
  );
};
