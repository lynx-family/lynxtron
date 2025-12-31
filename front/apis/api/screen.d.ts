import { Point } from '../structures/point';
import { Size } from '../structures/size';
import { Rectangle } from '../structures/rectangle';
import { BaseWindow } from './base-window';
import { EventEmitter } from 'node:events';

export interface Display {
  // Docs: https://electronjs.org/docs/api/structures/display

  /**
   * Can be `available`, `unavailable`, `unknown`.
   */
  accelerometerSupport: 'available' | 'unavailable' | 'unknown';
  /**
   * the bounds of the display in DIP points.
   */
  bounds: Rectangle;
  /**
   * The number of bits per pixel.
   */
  colorDepth: number;
  /**
   *  represent a color space (three-dimensional object which contains all realizable
   * color combinations) for the purpose of color conversions.
   */
  colorSpace: string;
  /**
   * The number of bits per color component.
   */
  depthPerComponent: number;
  /**
   * `true` if the display is detected by the system.
   */
  detected: boolean;
  /**
   * The display refresh rate.
   */
  displayFrequency: number;
  /**
   * Unique identifier associated with the display. A value of of -1 means the
   * display is invalid or the correct `id` is not yet known, and a value of -10
   * means the display is a virtual display assigned to a unified desktop.
   */
  id: number;
  /**
   * `true` for an internal display and `false` for an external display.
   */
  internal: boolean;
  /**
   * User-friendly label, determined by the platform.
   */
  label: string;
  /**
   * Maximum cursor size in native pixels.
   */
  maximumCursorSize: Size;
  /**
   * Whether or not the display is a monochrome display.
   */
  monochrome: boolean;
  /**
   * Returns the display's origin in pixel coordinates. Only available on windowing
   * systems like X11 that position displays in pixel coordinates.
   */
  nativeOrigin: Point;
  /**
   * Can be 0, 90, 180, 270, represents screen rotation in clock-wise degrees.
   */
  rotation: number;
  /**
   * Output device's pixel scale factor.
   */
  scaleFactor: number;
  size: Size;
  /**
   * Can be `available`, `unavailable`, `unknown`.
   */
  touchSupport: 'available' | 'unavailable' | 'unknown';
  /**
   * the work area of the display in DIP points.
   */
  workArea: Rectangle;
  /**
   * The size of the work area.
   */
  workAreaSize: Size;
}

export interface Screen extends EventEmitter {
  // Docs: https://electronjs.org/docs/api/screen

  /**
   * Emitted when `newDisplay` has been added.
   */
  on(
    event: 'display-added',
    listener: (event: Event, newDisplay: Display) => void
  ): this;
  off(
    event: 'display-added',
    listener: (event: Event, newDisplay: Display) => void
  ): this;
  once(
    event: 'display-added',
    listener: (event: Event, newDisplay: Display) => void
  ): this;
  addListener(
    event: 'display-added',
    listener: (event: Event, newDisplay: Display) => void
  ): this;
  removeListener(
    event: 'display-added',
    listener: (event: Event, newDisplay: Display) => void
  ): this;
  /**
   * Emitted when one or more metrics change in a `display`. The `changedMetrics` is
   * an array of strings that describe the changes. Possible changes are `bounds`,
   * `workArea`, `scaleFactor` and `rotation`.
   */
  on(
    event: 'display-metrics-changed',
    listener: (event: Event, display: Display, changedMetrics: string[]) => void
  ): this;
  off(
    event: 'display-metrics-changed',
    listener: (event: Event, display: Display, changedMetrics: string[]) => void
  ): this;
  once(
    event: 'display-metrics-changed',
    listener: (event: Event, display: Display, changedMetrics: string[]) => void
  ): this;
  addListener(
    event: 'display-metrics-changed',
    listener: (event: Event, display: Display, changedMetrics: string[]) => void
  ): this;
  removeListener(
    event: 'display-metrics-changed',
    listener: (event: Event, display: Display, changedMetrics: string[]) => void
  ): this;
  /**
   * Emitted when `oldDisplay` has been removed.
   */
  on(
    event: 'display-removed',
    listener: (event: Event, oldDisplay: Display) => void
  ): this;
  off(
    event: 'display-removed',
    listener: (event: Event, oldDisplay: Display) => void
  ): this;
  once(
    event: 'display-removed',
    listener: (event: Event, oldDisplay: Display) => void
  ): this;
  addListener(
    event: 'display-removed',
    listener: (event: Event, oldDisplay: Display) => void
  ): this;
  removeListener(
    event: 'display-removed',
    listener: (event: Event, oldDisplay: Display) => void
  ): this;
  /**
   * Converts a screen DIP point to a screen physical point. The DPI scale is
   * performed relative to the display containing the DIP point.
   *
   * Not currently supported on Wayland.
   *
   * @platform win32,linux
   */
  dipToScreenPoint(point: Point): Point;
  /**
   * Converts a screen DIP rect to a screen physical rect. The DPI scale is performed
   * relative to the display nearest to `window`. If `window` is null, scaling will
   * be performed to the display nearest to `rect`.
   *
   * @platform win32
   */
  dipToScreenRect(window: BaseWindow | null, rect: Rectangle): Rectangle;
  /**
   * An array of displays that are currently available.
   */
  getAllDisplays(): Display[];
  /**
   * The current absolute position of the mouse pointer.
   *
   * > [!NOTE] The return value is a DIP point, not a screen physical point.
   */
  getCursorScreenPoint(): Point;
  /**
   * The display that most closely intersects the provided bounds.
   */
  getDisplayMatching(rect: Rectangle): Display;
  /**
   * The display nearest the specified point.
   */
  getDisplayNearestPoint(point: Point): Display;
  /**
   * The primary display.
   */
  getPrimaryDisplay(): Display;
  /**
   * Converts a screen physical point to a screen DIP point. The DPI scale is
   * performed relative to the display containing the physical point.
   *
   * Not currently supported on Wayland - if used there it will return the point
   * passed in with no changes.
   *
   * @platform win32,linux
   */
  screenToDipPoint(point: Point): Point;
  /**
   * Converts a screen physical rect to a screen DIP rect. The DPI scale is performed
   * relative to the display nearest to `window`. If `window` is null, scaling will
   * be performed to the display nearest to `rect`.
   *
   * @platform win32
   */
  screenToDipRect(window: BaseWindow | null, rect: Rectangle): Rectangle;
}

export declare const screen: Screen;
