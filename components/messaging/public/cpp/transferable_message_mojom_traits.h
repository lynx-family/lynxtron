// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef COMPONENTS_MESSAGING_PUBLIC_CPP_TRANSFERABLE_MESSAGE_MOJOM_TRAITS_H_
#define COMPONENTS_MESSAGING_PUBLIC_CPP_TRANSFERABLE_MESSAGE_MOJOM_TRAITS_H_

#include <vector>

#include "components/messaging/public/cpp/cloneable_message_mojom_traits.h"
#include "components/messaging/public/cpp/message_port_descriptor_mojom_traits.h"
#include "components/messaging/public/cpp/transferable_message.h"
#include "components/messaging/public/mojom/transferable_message.mojom.h"

namespace mojo {

template <>
struct StructTraits<messaging::mojom::TransferableMessage::DataView,
                    messaging::TransferableMessage> {
  static messaging::CloneableMessage& message(
      messaging::TransferableMessage& input) {
    return input;
  }

  static std::vector<messaging::MessagePortDescriptor> ports(
      messaging::TransferableMessage& input) {
    return messaging::MessagePortChannel::ReleaseHandles(input.ports);
  }

  static bool Read(messaging::mojom::TransferableMessage::DataView data,
                   messaging::TransferableMessage* out);
};

}  // namespace mojo

#endif  // COMPONENTS_MESSAGING_PUBLIC_CPP_TRANSFERABLE_MESSAGE_MOJOM_TRAITS_H_
