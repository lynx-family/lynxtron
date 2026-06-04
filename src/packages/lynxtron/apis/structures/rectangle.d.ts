// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export interface Rectangle {
  /**
   * The height of the rectangle (must be an integer).
   */
  height: number;
  /**
   * The width of the rectangle (must be an integer).
   */
  width: number;
  /**
   * The x coordinate of the origin of the rectangle (must be an integer).
   */
  x: number;
  /**
   * The y coordinate of the origin of the rectangle (must be an integer).
   */
  y: number;
}
