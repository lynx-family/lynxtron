// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { EventEmitter } from 'events';

const { Tray } = process._linkedBinding('lynxtron_binding_tray');

Object.setPrototypeOf(Tray.prototype, EventEmitter.prototype);

const menuRefs = new WeakMap<object, object | null>();

const originalSetContextMenu = Tray.prototype.setContextMenu;
Tray.prototype.setContextMenu = function (menu: object | null) {
  menuRefs.set(this, menu);
  return originalSetContextMenu.call(this, menu);
};

const originalPopUpContextMenu = Tray.prototype.popUpContextMenu;
Tray.prototype.popUpContextMenu = function (
  menu?: object,
  position?: { x: number; y: number }
) {
  if (!menu && menuRefs.has(this)) {
    return originalPopUpContextMenu.call(this, menuRefs.get(this), position);
  }
  return originalPopUpContextMenu.call(this, menu, position);
};

export default Tray;
