export interface NotificationResponse {
  /**
   * The identifier string of the action that the user selected.
   */
  actionIdentifier: string;
  /**
   * The delivery date of the notification.
   */
  date: number;
  /**
   * The unique identifier for this notification request.
   */
  identifier: string;
  /**
   * A dictionary of custom information associated with the notification.
   */
  userInfo: Record<string, any>;
  /**
   * The text entered or chosen by the user.
   */
  userText?: string;
}
