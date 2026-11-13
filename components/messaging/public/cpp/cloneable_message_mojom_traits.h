// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef COMPONENTS_MESSAGING_PUBLIC_CPP_CLONEABLE_MESSAGE_MOJOM_TRAITS_H_
#define COMPONENTS_MESSAGING_PUBLIC_CPP_CLONEABLE_MESSAGE_MOJOM_TRAITS_H_

#include "components/messaging/public/cpp/cloneable_message.h"
#include "components/messaging/public/mojom/cloneable_message.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

template <>
struct StructTraits<messaging::mojom::CloneableMessage::DataView,
                    messaging::CloneableMessage> {
  static base::span<const uint8_t> encoded_message(
      messaging::CloneableMessage& input) {
    return input.encoded_message;
  }

  static bool Read(messaging::mojom::CloneableMessage::DataView data,
                   messaging::CloneableMessage* out);
};

}  // namespace mojo

#endif  // COMPONENTS_MESSAGING_PUBLIC_CPP_CLONEABLE_MESSAGE_MOJOM_TRAITS_H_
