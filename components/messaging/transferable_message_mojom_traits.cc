// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "components/messaging/public/cpp/transferable_message_mojom_traits.h"

#include "base/containers/span.h"
#include "components/messaging/public/cpp/cloneable_message_mojom_traits.h"
#include "components/messaging/public/cpp/message_port_descriptor_mojom_traits.h"

namespace mojo {

bool StructTraits<messaging::mojom::TransferableMessage::DataView,
                  messaging::TransferableMessage>::
    Read(messaging::mojom::TransferableMessage::DataView data,
         messaging::TransferableMessage* out) {
  std::vector<messaging::MessagePortDescriptor> ports;
  if (!data.ReadMessage(static_cast<messaging::CloneableMessage*>(out)) ||
      !data.ReadPorts(&ports)) {
    return false;
  }

  out->ports =
      messaging::MessagePortChannel::CreateFromHandles(std::move(ports));
  return true;
}

}  // namespace mojo
