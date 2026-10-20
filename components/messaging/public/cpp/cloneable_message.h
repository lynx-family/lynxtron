// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef COMPONENTS_MESSAGING_PUBLIC_CPP_CLONEABLE_MESSAGE_H_
#define COMPONENTS_MESSAGING_PUBLIC_CPP_CLONEABLE_MESSAGE_H_

#include <vector>

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace messaging {

// This struct represents messages as they are posted over a broadcast channel.
// This type can be serialized as a messaging::mojom::CloneableMessage struct.
struct CloneableMessage {
  CloneableMessage();
  CloneableMessage(CloneableMessage&&);
  CloneableMessage& operator=(CloneableMessage&&);
  ~CloneableMessage();

  // Returns a shallow clone of this message. |encoded_message| in the clone
  // will point to the same memory as |encoded_message| in this.
  CloneableMessage ShallowClone() const;

  // To reduce copies when serializing |encoded_message| does not have to point
  // to |owned_encoded_message|. The serialization code completely ignores the
  // |owned_encoded_message| and just serializes whatever |encoded_message|
  // points to. When deserializing |owned_encoded_message| is set to the data
  // and |encoded_message| is set to point to |owned_encoded_message|.
  base::span<const uint8_t> encoded_message;
  std::vector<uint8_t> owned_encoded_message;

  // Copies data into owned_encoded_message if it's not already there.
  void EnsureDataIsOwned();
};

}  // namespace messaging

#endif  // COMPONENTS_MESSAGING_PUBLIC_CPP_CLONEABLE_MESSAGE_H_
