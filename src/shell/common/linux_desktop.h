// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License, Version 2.0.
#ifndef LYNXTRON_SHELL_COMMON_LINUX_DESKTOP_H_
#define LYNXTRON_SHELL_COMMON_LINUX_DESKTOP_H_
#include <cstdint>
#include <memory>
namespace lynx::pub {
class LynxWindowlessRenderer;
}
namespace lynxtron {
class NativeWindow;
class LynxtronMenuModel;
void SetLinuxWindowMenu(NativeWindow* window, LynxtronMenuModel* model);
void ApplyLinuxApplicationMenu(NativeWindow* window);
void RefreshLinuxMenus();
void SetLinuxApplicationMenu(LynxtronMenuModel* model);
void* CreateLinuxMenu(LynxtronMenuModel* model, bool menubar = false);
// GTK is optional for headless Node/windowless applications.
void* GetLinuxContentBox(NativeWindow* window);
uintptr_t GetLinuxNativeWindowId(NativeWindow* window);
void InitializeLinuxDesktop();
bool EnsureLinuxDesktop();
std::shared_ptr<lynx::pub::LynxWindowlessRenderer> CreateLinuxDesktopRenderer(
    NativeWindow* window);
}  // namespace lynxtron
#endif
