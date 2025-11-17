
#include "lynxtron/shell/common/win_util.h"

#include <objbase.h>

#include <initguid.h>
#include <shobjidl.h>  // Must be before propkey.

#include <aclapi.h>
#include <cfgmgr32.h>
#include <inspectable.h>
#include <lm.h>
#include <mdmregistration.h>
#include <powrprof.h>
#include <propkey.h>
#include <psapi.h>
#include <roapi.h>
#include <sddl.h>
#include <setupapi.h>
#include <shellscalingapi.h>
// #include <signal.h>
#include <tchar.h>  // Must be before tpcshrd.h or for any use of _T macro

#include <stddef.h>
#include <stdlib.h>
#include <strsafe.h>
#include <tpcshrd.h>
#include <uiviewsettingsinterop.h>
#include <windows.ui.viewmanagement.h>
#include <winstring.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

namespace lynxtron {
bool IsProcessPerMonitorDpiAware() {
  enum class PerMonitorDpiAware {
    UNKNOWN = 0,
    PER_MONITOR_DPI_UNAWARE,
    PER_MONITOR_DPI_AWARE,
  };
  static PerMonitorDpiAware per_monitor_dpi_aware = PerMonitorDpiAware::UNKNOWN;
  if (per_monitor_dpi_aware == PerMonitorDpiAware::UNKNOWN) {
    per_monitor_dpi_aware = PerMonitorDpiAware::PER_MONITOR_DPI_UNAWARE;
    HMODULE shcore_dll = ::LoadLibrary(L"shcore.dll");
    if (shcore_dll) {
      auto get_process_dpi_awareness_func =
          reinterpret_cast<decltype(::GetProcessDpiAwareness)*>(
              ::GetProcAddress(shcore_dll, "GetProcessDpiAwareness"));
      if (get_process_dpi_awareness_func) {
        PROCESS_DPI_AWARENESS awareness;
        if (SUCCEEDED(get_process_dpi_awareness_func(nullptr, &awareness)) &&
            awareness == PROCESS_PER_MONITOR_DPI_AWARE) {
          per_monitor_dpi_aware = PerMonitorDpiAware::PER_MONITOR_DPI_AWARE;
        }
      }
    }
  }
  return per_monitor_dpi_aware == PerMonitorDpiAware::PER_MONITOR_DPI_AWARE;
}
}  // namespace lynxtron
