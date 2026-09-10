#include "shared/elements/ConsumerWidgetElement.h"

#ifndef LYNX_NATIVE_ELEMENT_BACKEND_TEXTURE
#define LYNX_NATIVE_ELEMENT_BACKEND_TEXTURE 1
#endif

lynx_native_view_t* CreateConsumerWidgetElementTextureNativeView(void* opaque);
lynx_native_view_t* CreateConsumerWidgetElementNativeUINativeView(void* opaque);

lynx_native_view_t* CreateConsumerWidgetElementNativeView(void* opaque) {
  // Both element backend implementations are generated intentionally:
  // texture mode is better for compositor-owned rendering, while native-ui mode
  // is useful when embedding platform controls. Switch with
  // -DLYNX_NATIVE_ELEMENT_BACKEND=native-ui when configuring CMake.
#if LYNX_NATIVE_ELEMENT_BACKEND_TEXTURE
  return CreateConsumerWidgetElementTextureNativeView(opaque);
#else
  return CreateConsumerWidgetElementNativeUINativeView(opaque);
#endif
}
