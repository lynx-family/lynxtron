// Window geometry in px, relative to the display origin.
export interface WindowBounds {
  left: number;
  top: number;
  width: number;
  height: number;
}

// Options passed from C++ NativeWindowHarmony via the NAPI bridge.
// Mirrors lynxtron::HarmonyWindowCreationOptions in C++.
export type WindowType = 'main' | 'sub' | 'float' | 'panel' | 'dialog';

// Creation parameters for one C++-driven window request.
export interface CreateWindowOptions {
  windowId: number;
  type: WindowType;
  x?: number;
  y?: number;
  width?: number;
  height?: number;
  show?: boolean;
  title?: string;
  resizable?: boolean;
  movable?: boolean;
  minimizable?: boolean;
  maximizable?: boolean;
  closable?: boolean;
  focusable?: boolean;
  alwaysOnTop?: boolean;
  parentWindowId?: number;
  parentHarmonyWindowId?: number;
  center?: boolean;
  hasX?: boolean;
  hasY?: boolean;
  fullscreen?: boolean;
  minWidth?: number;
  minHeight?: number;
  maxWidth?: number;
  maxHeight?: number;
  modal?: boolean;
  displayId?: number;
  // Internal retry counter. Parent Ability creation is asynchronous, while
  // Electron-compatible JS commonly creates parent and child back-to-back.
  parentResolveAttempt?: number;
}
