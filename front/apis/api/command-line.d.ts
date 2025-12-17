export declare class CommandLine {
  // Docs: https://electronjs.org/docs/api/command-line

  /**
   * Append an argument to Chromium's command line. The argument will be quoted
   * correctly. Switches will precede arguments regardless of appending order.
   *
   * If you're appending an argument like `--switch=value`, consider using
   * `appendSwitch('switch', 'value')` instead.
   *
   * [!NOTE] This will not affect `process.argv`. The intended usage of this
   * function is to control Chromium's behavior.
   */
  appendArgument(value: string): void;
  /**
   * Append a switch (with optional `value`) to Chromium's command line.
   *
   * [!NOTE] This will not affect `process.argv`. The intended usage of this
   * function is to control Chromium's behavior.
   */
  appendSwitch(the_switch: string, value?: string): void;
  /**
   * The command-line switch value.
   *
   * This function is meant to obtain Chromium command line switches. It is not meant
   * to be used for application-specific command line arguments. For the latter,
   * please use `process.argv`.
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
   * Removes the specified switch from Chromium's command line.
   *
   * > [!NOTE] This will not affect `process.argv`. The intended usage of this
   * function is to control Chromium's behavior.
   */
  removeSwitch(the_switch: string): void;
}
