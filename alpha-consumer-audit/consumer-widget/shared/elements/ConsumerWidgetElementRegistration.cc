#include <lynx/registration.h>

#include "shared/elements/ConsumerWidgetElement.h"

LYNX_REGISTER_ELEMENT(
    "WidgetElementModule",
    "x-consumer-widget",
    CreateConsumerWidgetElementNativeView,
    false,
    nullptr)
