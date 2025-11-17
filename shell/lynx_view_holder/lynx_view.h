#ifndef LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_H_
#define LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "shell/lynx_view_holder/lynx_view_client.h"
#include "shell/ui/gfx/geometry/rect.h"

namespace lynxtron {
class LynxViewImpl;

class LynxView {
 public:
  static std::unique_ptr<LynxView> Create();
  ~LynxView();

  void Init(double width, double height, float dpi, void* parent);
  void LoadTemplate(std::string_view template_url, base::span<uint8_t> content);
  void SetClient(base::WeakPtr<LynxViewClient> client);
  void SetBounds(const gfx::Rect& bounds);
  void Show();
  void Hide();
  void Close();
  void SendGlobalEvent(const std::string& event, const std::string& json);
  void UpdateData(const std::string& data, const std::string& global_props);
  void ReloadTemplate(const std::string& data, const std::string& global_props);

 private:
  LynxView();

  std::shared_ptr<LynxViewImpl> impl_;
};

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_LYNX_VIEW_HOLDER_LYNX_VIEW_H_
