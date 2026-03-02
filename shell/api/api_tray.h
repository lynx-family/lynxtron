// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef LYNXTRON_SHELL_API_API_TRAY_H_
#define LYNXTRON_SHELL_API_API_TRAY_H_

#include <memory>
#include <string>
#include <vector>

#include "../common/gin_helper/arguments.h"
#include "../common/gin_helper/constructible.h"
#include "../common/gin_helper/object_template_builder.h"
#include "../common/gin_helper/wrappable.h"
#include "../ui/gfx/geometry/point.h"
#include "../ui/gfx/geometry/rect.h"
#include "../ui/tray_icon.h"
#include "event_emitter_mixin.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace lynxtron {

namespace api {

class Menu;
class NativeImage;

class Tray final : public gin_helper::DeprecatedWrappable<Tray>,
                   public gin_helper::EventEmitterMixin<Tray>,
                   public gin_helper::Constructible<Tray>,
                   public TrayIconObserver {
 public:
  static gin::DeprecatedWrapperInfo kWrapperInfo;
  static Tray* New(gin::Arguments* args);

  Tray(gin::Arguments* args);
  ~Tray() override;

  void Destroy();
  bool IsDestroyed() const;
  void SetImage(gin::Arguments* args);
  void SetPressedImage(gin::Arguments* args);
  void SetToolTip(const std::string& tool_tip);
  void SetTitle(const std::string& title);
  std::string GetTitle() const;
  void SetIgnoreDoubleClickEvents(bool ignore);
  bool GetIgnoreDoubleClickEvents() const;
  void SetContextMenu(gin::Arguments* args);
  void PopUpContextMenu(gin::Arguments* args);
  void CloseContextMenu();
  gfx::Rect GetBounds() const;
  v8::Local<v8::Value> GetGUID(gin::Arguments* args);
  void DisplayBalloon(gin::Arguments* args);
  void RemoveBalloon();
  void Focus();

  const gin::DeprecatedWrapperInfo* wrapper_info() const;
  const char* GetTypeName() override;
  static void FillObjectTemplate(v8::Isolate* isolate,
                                 v8::Local<v8::ObjectTemplate> templ);
  static const char* GetClassName() { return "Tray"; }

 private:
  void EmitClick(const TrayBounds& bounds, const TrayPoint& position);
  void EmitMouseEvent(const char* name, const TrayPoint& position);
  gfx::Rect ToGfxRect(const TrayBounds& bounds) const;
  gfx::Point ToGfxPoint(const TrayPoint& point) const;

  Menu* GetMenuFromValue(v8::Isolate* isolate, v8::Local<v8::Value> menu);
  void SetMenu(Menu* menu, v8::Isolate* isolate, v8::Local<v8::Value> value);

  void OnClick(const TrayBounds& bounds, const TrayPoint& position) override;
  void OnRightClick(const TrayBounds& bounds) override;
  void OnDoubleClick(const TrayBounds& bounds) override;
  void OnMiddleClick(const TrayBounds& bounds) override;
  void OnMouseDown(const TrayPoint& position) override;
  void OnMouseUp(const TrayPoint& position) override;
  void OnMouseEnter(const TrayPoint& position) override;
  void OnMouseLeave(const TrayPoint& position) override;
  void OnMouseMove(const TrayPoint& position) override;
  void OnBalloonShow() override;
  void OnBalloonClick() override;
  void OnBalloonClosed() override;
  void OnDrop() override;
  void OnDropFiles(const std::vector<std::string>& files) override;
  void OnDropText(const std::string& text) override;
  void OnDragEnter() override;
  void OnDragLeave() override;
  void OnDragEnd() override;

  bool destroyed_ = false;
  Menu* menu_ = nullptr;
  v8::Global<v8::Object> menu_handle_;
  std::unique_ptr<TrayIcon> tray_icon_;
};

}  // namespace api

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_API_API_TRAY_H_
