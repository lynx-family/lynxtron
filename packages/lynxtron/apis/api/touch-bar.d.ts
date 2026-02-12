import { NativeImage } from './native-image';

export interface ScrubberItem {
  icon?: NativeImage;
  label?: string;
}

export interface SegmentedControlSegment {
  enabled?: boolean;
  icon?: NativeImage;
  label?: string;
}

export interface TouchBarButtonConstructorOptions {
  label?: string;
  accessibilityLabel?: string;
  backgroundColor?: string;
  icon?: NativeImage | string;
  iconPosition?: 'left' | 'right' | 'overlay';
  click?: () => void;
  enabled?: boolean;
}

export interface TouchBarColorPickerConstructorOptions {
  availableColors?: string[];
  selectedColor?: string;
  change?: (color: string) => void;
}

export interface TouchBarConstructorOptions {
  items?: Array<
    | TouchBarButton
    | TouchBarColorPicker
    | TouchBarGroup
    | TouchBarLabel
    | TouchBarPopover
    | TouchBarScrubber
    | TouchBarSegmentedControl
    | TouchBarSlider
    | TouchBarSpacer
  >;
  escapeItem?:
    | TouchBarButton
    | TouchBarColorPicker
    | TouchBarGroup
    | TouchBarLabel
    | TouchBarPopover
    | TouchBarScrubber
    | TouchBarSegmentedControl
    | TouchBarSlider
    | TouchBarSpacer
    | null;
}

export interface TouchBarGroupConstructorOptions {
  items: TouchBar;
}

export interface TouchBarLabelConstructorOptions {
  label?: string;
  accessibilityLabel?: string;
  textColor?: string;
}

export interface TouchBarPopoverConstructorOptions {
  label?: string;
  icon?: NativeImage;
  items: TouchBar;
  showCloseButton?: boolean;
}

export interface TouchBarScrubberConstructorOptions {
  items: ScrubberItem[];
  select?: (selectedIndex: number) => void;
  highlight?: (highlightedIndex: number) => void;
  selectedStyle?: 'background' | 'outline' | 'none';
  overlayStyle?: 'background' | 'outline' | 'none';
  showArrowButtons?: boolean;
  mode?: 'fixed' | 'free';
  continuous?: boolean;
}

export interface TouchBarSegmentedControlConstructorOptions {
  segmentStyle?:
    | 'automatic'
    | 'rounded'
    | 'textured-rounded'
    | 'round-rect'
    | 'textured-square'
    | 'capsule'
    | 'small-square'
    | 'separated';
  mode?: 'single' | 'multiple' | 'buttons';
  segments: SegmentedControlSegment[];
  selectedIndex?: number;
  change?: (selectedIndex: number, isSelected: boolean) => void;
}

export interface TouchBarSliderConstructorOptions {
  label?: string;
  value?: number;
  minValue?: number;
  maxValue?: number;
  change?: (newValue: number) => void;
}

export interface TouchBarSpacerConstructorOptions {
  size?: 'small' | 'large' | 'flexible';
}

export declare class TouchBar {
  constructor(options: TouchBarConstructorOptions);
  escapeItem:
    | TouchBarButton
    | TouchBarColorPicker
    | TouchBarGroup
    | TouchBarLabel
    | TouchBarPopover
    | TouchBarScrubber
    | TouchBarSegmentedControl
    | TouchBarSlider
    | TouchBarSpacer
    | null;
  static TouchBarButton: typeof TouchBarButton;
  static TouchBarColorPicker: typeof TouchBarColorPicker;
  static TouchBarGroup: typeof TouchBarGroup;
  static TouchBarLabel: typeof TouchBarLabel;
  static TouchBarOtherItemsProxy: typeof TouchBarOtherItemsProxy;
  static TouchBarPopover: typeof TouchBarPopover;
  static TouchBarScrubber: typeof TouchBarScrubber;
  static TouchBarSegmentedControl: typeof TouchBarSegmentedControl;
  static TouchBarSlider: typeof TouchBarSlider;
  static TouchBarSpacer: typeof TouchBarSpacer;
}

export declare class TouchBarButton {
  constructor(options: TouchBarButtonConstructorOptions);
  accessibilityLabel: string;
  backgroundColor: string;
  enabled: boolean;
  icon: NativeImage;
  iconPosition: 'left' | 'right' | 'overlay';
  label: string;
}

export declare class TouchBarColorPicker {
  constructor(options: TouchBarColorPickerConstructorOptions);
  availableColors: string[];
  selectedColor: string;
}

export declare class TouchBarGroup {
  constructor(options: TouchBarGroupConstructorOptions);
}

export declare class TouchBarLabel {
  constructor(options: TouchBarLabelConstructorOptions);
  accessibilityLabel: string;
  label: string;
  textColor: string;
}

export declare class TouchBarOtherItemsProxy {
  constructor();
}

export declare class TouchBarPopover {
  constructor(options: TouchBarPopoverConstructorOptions);
  icon: NativeImage;
  label: string;
}

export declare class TouchBarScrubber {
  constructor(options: TouchBarScrubberConstructorOptions);
  continuous: boolean;
  items: ScrubberItem[];
  mode: 'fixed' | 'free';
  overlayStyle: 'background' | 'outline' | 'none';
  selectedStyle: 'background' | 'outline' | 'none';
  showArrowButtons: boolean;
}

export declare class TouchBarSegmentedControl {
  constructor(options: TouchBarSegmentedControlConstructorOptions);
  mode: 'single' | 'multiple' | 'buttons';
  segments: SegmentedControlSegment[];
  segmentStyle: string;
  selectedIndex: number;
}

export declare class TouchBarSlider {
  constructor(options: TouchBarSliderConstructorOptions);
  label: string;
  maxValue: number;
  minValue: number;
  value: number;
}

export declare class TouchBarSpacer {
  constructor(options: TouchBarSpacerConstructorOptions);
  size: 'small' | 'large' | 'flexible';
}
