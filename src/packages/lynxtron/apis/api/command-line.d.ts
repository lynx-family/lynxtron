// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

export declare class CommandLine {
  /**
   * Append an argument to the command line. The argument will be quoted correctly.
   * Switches will precede arguments regardless of appending order.
   *
   * If you're appending an argument like `--switch=value`, consider using
   * `appendSwitch('switch', 'value')` instead.
   *
   * [!NOTE] This will not affect `process.argv`. The intended usage of this
   * function is to control command-line behavior.
   */
  appendArgument(value: string): void;
  /**
   * Append a switch (with optional `value`) to the command line.
   *
   * [!NOTE] This will not affect `process.argv`. The intended usage of this
   * function is to control command-line behavior.
   */
  appendSwitch(the_switch: string, value?: string): void;
  /**
   * The command-line switch value.
   *
   * This function is meant to obtain command-line switches. It is not meant to be
   * used for application-specific command line arguments. For the latter, please use
   * `process.argv`.
   *
   * > [!NOTE] When the switch is not present or has no value, it returns empty
   * string.
   */
  getSwitchValue(the_switch: string): string;
  /**
   * Whether the command-line switch is present.
   */
  hasSwitch(the_switch: string): boolean;
  /**
   * Removes the specified switch from the command line.
   *
   * > [!NOTE] This will not affect `process.argv`. The intended usage of this
   * function is to control command-line behavior.
   */
  removeSwitch(the_switch: string): void;
}
