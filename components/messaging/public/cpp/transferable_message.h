// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef COMPONENTS_MESSAGING_PUBLIC_CPP_TRANSFERABLE_MESSAGE_H_
#define COMPONENTS_MESSAGING_PUBLIC_CPP_TRANSFERABLE_MESSAGE_H_

#include <vector>

#include "components/messaging/public/cpp/cloneable_message.h"
#include "components/messaging/public/cpp/message_port_channel.h"

namespace messaging {

// This struct represents messages as they are posted over a message port. This
// type can be serialized as a messaging::mojom::TransferableMessage struct.
struct TransferableMessage : public CloneableMessage {
  TransferableMessage();
  TransferableMessage(TransferableMessage&&);
  TransferableMessage& operator=(TransferableMessage&&);
  ~TransferableMessage();

  // Any ports being transferred as part of this message.
  std::vector<MessagePortChannel> ports;
};

}  // namespace messaging

#endif  // COMPONENTS_MESSAGING_PUBLIC_CPP_TRANSFERABLE_MESSAGE_H_
