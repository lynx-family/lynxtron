import { LynxWindow } from './LynxWindow';

// Subwindows reuse the same XComponent / native-bridge component as main
// windows. The component detects the subwindow via the window name prefix
// "lynxtron-sub-<cppWindowId>" and resolves the correct origin window id.
@Component
export struct LynxSubWindow {
  build() {
    LynxWindow()
  }
}
