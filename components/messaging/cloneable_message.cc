// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "components/messaging/public/cpp/cloneable_message.h"

namespace messaging {

CloneableMessage::CloneableMessage() = default;
CloneableMessage::CloneableMessage(CloneableMessage&&) = default;
CloneableMessage& CloneableMessage::operator=(CloneableMessage&&) = default;
CloneableMessage::~CloneableMessage() = default;

CloneableMessage CloneableMessage::ShallowClone() const {
  CloneableMessage clone;
  clone.encoded_message = encoded_message;
  return clone;
}

void CloneableMessage::EnsureDataIsOwned() {
  if (encoded_message.data() == owned_encoded_message.data()) {
    return;
  }
  owned_encoded_message.assign(encoded_message.begin(), encoded_message.end());
  encoded_message = owned_encoded_message;
}

}  // namespace messaging
