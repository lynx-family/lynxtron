export interface Task {
  // Docs: https://electronjs.org/docs/api/structures/task

  /**
   * The command line arguments when `program` is executed.
   */
  arguments: string;
  /**
   * Description of this task.
   */
  description: string;
  /**
   * The icon index in the icon file. If an icon file consists of two or more icons,
   * set this value to identify the icon. If an icon file consists of one icon, this
   * value is 0.
   */
  iconIndex: number;
  /**
   * The absolute path to an icon to be displayed in a JumpList, which can be an
   * arbitrary resource file that contains an icon. You can usually specify
   * `process.execPath` to show the icon of the program.
   */
  iconPath: string;
  /**
   * Path of the program to execute, usually you should specify `process.execPath`
   * which opens the current program.
   */
  program: string;
  /**
   * The string to be displayed in a JumpList.
   */
  title: string;
  /**
   * The working directory. Default is empty.
   */
  workingDirectory?: string;
}
