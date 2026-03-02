import { EventEmitter } from 'node:events';

export interface NotificationAction {
  /**
   * The label for the given action.
   */
  text?: string;
  /**
   * The type of action, can be `button`.
   */
  type: 'button';
}

export interface NotificationConstructorOptions {
  /**
   * A title for the notification, which will be shown at the top of the notification
   * window when it is shown.
   */
  title?: string;
  /**
   * A subtitle for the notification, which will be displayed below the title.
   *
   * @platform darwin
   */
  subtitle?: string;
  /**
   * The body text of the notification, which will be displayed below the title or
   * subtitle.
   */
  body?: string;
  /**
   * Whether to show the notification without sound.
   */
  silent?: boolean;
  /**
   * An icon to use in the notification.
   */
  icon?: string;
  /**
   * Whether or not to add a reply option to the notification.
   *
   * @platform darwin
   */
  hasReply?: boolean;
  /**
   * The timeout duration of the notification. Can be 'default' or 'never'.
   *
   * @platform win32
   */
  timeoutType?: 'default' | 'never';
  /**
   * The placeholder to write in the reply input field.
   *
   * @platform darwin
   */
  replyPlaceholder?: string;
  /**
   * The name of the sound file to play when the notification is shown.
   *
   * @platform darwin
   */
  sound?: string;
  /**
   * The urgency level of the notification. Can be 'normal', 'critical', or 'low'.
   *
   * @platform linux
   */
  urgency?: 'normal' | 'critical' | 'low';
  /**
   * Actions to add to the notification.
   *
   * @platform darwin
   */
  actions?: NotificationAction[];
  /**
   * A custom title for the close button of an alert. An empty string will cause the
   * default localized text to be used.
   *
   * @platform darwin
   */
  closeButtonText?: string;
  /**
   * A custom XML string to use for the toast notification.
   *
   * @platform win32
   */
  toastXml?: string;
}

export declare class Notification extends EventEmitter {
  /**
   * A `NotificationAction[]` property representing the actions of the notification.
   */
  actions: NotificationAction[];
  /**
   * A `string` property representing the body of the notification.
   */
  body: string;
  /**
   * A `string` property representing the close button text of the notification.
   */
  closeButtonText: string;
  /**
   * A `boolean` property representing whether the notification has a reply action.
   */
  hasReply: boolean;
  /**
   * A `string` property representing the reply placeholder of the notification.
   */
  replyPlaceholder: string;
  /**
   * A `boolean` property representing whether the notification is silent.
   */
  silent: boolean;
  /**
   * A `string` property representing the sound of the notification.
   */
  sound: string;
  /**
   * A `string` property representing the subtitle of the notification.
   */
  subtitle: string;
  /**
   * A `string` property representing the type of timeout duration for the
   * notification. Can be 'default' or 'never'.
   *
   * If `timeoutType` is set to 'never', the notification never expires. It stays
   * open until closed by the calling API or the user.
   *
   * @platform win32
   */
  timeoutType: 'default' | 'never';
  /**
   * A `string` property representing the title of the notification.
   */
  title: string;
  /**
   * A `string` property representing the custom Toast XML of the notification.
   *
   * @platform win32
   */
  toastXml: string;

  constructor(options?: NotificationConstructorOptions);

  /**
   * Immediately shows the notification to the user.
   */
  public show(): void;

  /**
   * Dismisses the notification.
   */
  public close(): void;

  /**
   * Whether or not desktop notifications are supported on the current system.
   */
  public static isSupported(): boolean;

  /**
   * Emitted when the notification is shown to the user.
   */
  on(event: 'show', listener: () => void): this;
  off(event: 'show', listener: () => void): this;
  once(event: 'show', listener: () => void): this;
  addListener(event: 'show', listener: () => void): this;
  removeListener(event: 'show', listener: () => void): this;

  /**
   * Emitted when the notification is clicked by the user.
   */
  on(event: 'click', listener: () => void): this;
  off(event: 'click', listener: () => void): this;
  once(event: 'click', listener: () => void): this;
  addListener(event: 'click', listener: () => void): this;
  removeListener(event: 'click', listener: () => void): this;

  /**
   * Emitted when the notification is closed by manual intervention from the user.
   */
  on(event: 'close', listener: () => void): this;
  off(event: 'close', listener: () => void): this;
  once(event: 'close', listener: () => void): this;
  addListener(event: 'close', listener: () => void): this;
  removeListener(event: 'close', listener: () => void): this;

  /**
   * Emitted when the user clicks the "Reply" button on a notification with `hasReply: true`.
   *
   * @platform darwin
   */
  on(event: 'reply', listener: (event: Event, reply: string) => void): this;
  off(event: 'reply', listener: (event: Event, reply: string) => void): this;
  once(event: 'reply', listener: (event: Event, reply: string) => void): this;
  addListener(
    event: 'reply',
    listener: (event: Event, reply: string) => void
  ): this;
  removeListener(
    event: 'reply',
    listener: (event: Event, reply: string) => void
  ): this;

  /**
   * Emitted when the user clicks one of the actions on a notification.
   *
   * @platform darwin
   */
  on(event: 'action', listener: (event: Event, index: number) => void): this;
  off(event: 'action', listener: (event: Event, index: number) => void): this;
  once(event: 'action', listener: (event: Event, index: number) => void): this;
  addListener(
    event: 'action',
    listener: (event: Event, index: number) => void
  ): this;
  removeListener(
    event: 'action',
    listener: (event: Event, index: number) => void
  ): this;

  /**
   * Emitted when an error is encountered while showing the notification.
   *
   * @platform win32
   */
  on(event: 'failed', listener: (event: Event, error: string) => void): this;
  off(event: 'failed', listener: (event: Event, error: string) => void): this;
  once(event: 'failed', listener: (event: Event, error: string) => void): this;
  addListener(
    event: 'failed',
    listener: (event: Event, error: string) => void
  ): this;
  removeListener(
    event: 'failed',
    listener: (event: Event, error: string) => void
  ): this;
}
