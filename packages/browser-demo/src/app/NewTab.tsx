import type { FunctionComponent } from '@lynx-js/react';
import LynxTronIcon from './icons/lynxtron.png?inline';
import LynxIcon from './icons/lynxlogo.png?inline';
import CloseIcon from './icons/close.png?inline';

export const NewTab: FunctionComponent<{
  tabId?: string;
  isSelected?: boolean;
  onTap?: () => void;
  onClose?: () => void;
  icon?: 'lynx' | 'lynxtron';
  title?: string;
}> = ({
  tabId = 'default',
  isSelected = false,
  onTap,
  onClose,
  icon = 'lynxtron',
  title = 'New Tab',
}) => {
  const handleClose = () => {
    console.log('NewTab handleClose called with tabId:', tabId);
    if (onClose) {
      onClose();
    }
  };

  return (
    <view
      clip-radius="true"
      style={{
        backgroundColor: isSelected ? 'white' : '#ebf0f5',
        position: 'relative',
      }}
      className="search-box"
    >
      <view
        bindtap={onTap}
        style={{ display: 'flex', minWidth: '15px', flexGrow: 1 }}
      >
        <image
          src={icon === 'lynx' ? LynxIcon : LynxTronIcon}
          style={
            icon === 'lynx'
              ? { top: '1px' }
              : { width: '14px', height: '14px', top: '1px' }
          }
          className="search-start-icon"
        />
        <text className="search-title">{title}</text>
      </view>
      <view className="search-end-icon-container" bindtap={handleClose}>
        <image src={CloseIcon} className="search-end-icon" />
      </view>
    </view>
  );
};
