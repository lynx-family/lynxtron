export interface JumpListCategory {
  // Docs: https://electronjs.org/docs/api/structures/jump-list-category

  /**
   * Array of `JumpListItem` objects if `type` is `tasks` or `custom`, otherwise it
   * should be omitted.
   */
  items?: JumpListItem[];
  /**
   * Must be set if `type` is `custom`, otherwise it should be omitted.
   */
  name?: string;
  /**
   * One of the following:
   */
  type?: 'tasks' | 'frequent' | 'recent' | 'custom';
}

export interface JumpListSettings {
  /**
   * The minimum number of items that will be shown in the Jump List (for a more
   * detailed description of this value see the MSDN docs).
   */
  minItems: number;
  /**
   * Array of `JumpListItem` objects that correspond to items that the user has
   * explicitly removed from custom categories in the Jump List. These items must not
   * be re-added to the Jump List in the **next** call to `app.setJumpList()`,
   * Windows will not display any custom category that contains any of the removed
   * items.
   */
  removedItems: JumpListItem[];
}

export interface JumpListItem {
  // Docs: https://electronjs.org/docs/api/structures/jump-list-item

  /**
   * The command line arguments when `program` is executed. Should only be set if
   * `type` is `task`.
   */
  args?: string;
  /**
   * Description of the task (displayed in a tooltip). Should only be set if `type`
   * is `task`. Maximum length 260 characters.
   */
  description?: string;
  /**
   * The index of the icon in the resource file. If a resource file contains multiple
   * icons this value can be used to specify the zero-based index of the icon that
   * should be displayed for this task. If a resource file contains only one icon,
   * this property should be set to zero.
   */
  iconIndex?: number;
  /**
   * The absolute path to an icon to be displayed in a Jump List, which can be an
   * arbitrary resource file that contains an icon (e.g. `.ico`, `.exe`, `.dll`). You
   * can usually specify `process.execPath` to show the program icon.
   */
  iconPath?: string;
  /**
   * Path of the file to open, should only be set if `type` is `file`.
   */
  path?: string;
  /**
   * Path of the program to execute, usually you should specify `process.execPath`
   * which opens the current program. Should only be set if `type` is `task`.
   */
  program?: string;
  /**
   * The text to be displayed for the item in the Jump List. Should only be set if
   * `type` is `task`.
   */
  title?: string;
  /**
   * One of the following:
   */
  type?: 'task' | 'separator' | 'file';
  /**
   * The working directory. Default is empty.
   */
  workingDirectory?: string;
}
