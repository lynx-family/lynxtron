/// <reference types="@lynx-js/types" />
import type {} from '@lynx-js/lynxtron/lynx';

declare module '@lynx-js/lynxtron/lynx' {
  interface LynxtronNodejsExposed {
    config: { darkMode: boolean };
    readSetting(key: 'theme' | 'locale'): Promise<string>;
    setBadgeCount(count: number): void;
  }
}

NativeModules.LynxTestModule?.runTest?.('still-lynx-native-modules');

function expectBoolean(value: boolean): void {
  void value;
}

function expectString(value: string): void {
  void value;
}

NativeModules.bridge.call('settings:get', { key: 'theme' }, (value) => {
  const result: unknown = value;
  void result;
});

NativeModules.bridge.on('settings:changed', (...args) => {
  const key: unknown = args[0];
  void key;
});

expectBoolean(NativeModules.nodejs.exposed.config.darkMode);
NativeModules.nodejs.exposed.readSetting('theme').then(expectString);
NativeModules.nodejs.exposed.setBadgeCount(1);

// @ts-expect-error readSetting only accepts declared setting keys.
NativeModules.nodejs.exposed.readSetting('missing');

// @ts-expect-error setBadgeCount requires a number.
NativeModules.nodejs.exposed.setBadgeCount('1');

// @ts-expect-error Lynx bridge.call requires params and callback.
NativeModules.bridge.call('app:ping', () => {});
