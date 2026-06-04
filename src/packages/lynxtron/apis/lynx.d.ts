import '@lynx-js/types/background';

export interface LynxtronNodejsExposed {
  [key: string]: unknown;
}

export interface LynxtronNodejs {
  exposed: LynxtronNodejsExposed;
}

declare module '@lynx-js/types/background' {
  interface NativeModules {
    nodejs: LynxtronNodejs;
  }
}
