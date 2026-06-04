import { LynxWindow } from '@lynx-js/lynxtron';
import type {
  LynxBridgeInvokeListener,
  LynxBridgeMessageListener,
} from '@lynx-js/lynxtron';

const win = new LynxWindow({ width: 800, height: 600 });

const invokeListener: LynxBridgeInvokeListener = (
  event,
  methodName,
  params
) => {
  const channel: string = methodName;
  const payload: unknown = params;
  void channel;
  void payload;
  event.sendReply({ ok: true });
};

const messageListener: LynxBridgeMessageListener = (methodName, params) => {
  const channel: string = methodName;
  const payload: unknown = params;
  void channel;
  void payload;
};

win.on('-lynx-invoke', invokeListener);
win.on('-lynx-message', messageListener);

const invalidInvokeListener: LynxBridgeInvokeListener = (
  _event,
  _methodName,
  params
) => {
  // @ts-expect-error params are unknown until narrowed.
  return params.name;
};
void invalidInvokeListener;

const invalidMessageListener: LynxBridgeMessageListener = (
  _methodName,
  params
) => {
  // @ts-expect-error params are unknown until narrowed.
  return params.name;
};
void invalidMessageListener;
