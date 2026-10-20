// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "components/messaging/public/cpp/cloneable_message_mojom_traits.h"

namespace mojo {

bool StructTraits<messaging::mojom::CloneableMessage::DataView,
                  messaging::CloneableMessage>::
    Read(messaging::mojom::CloneableMessage::DataView data,
         messaging::CloneableMessage* out) {
  if (!data.ReadEncodedMessage(&out->owned_encoded_message)) {
    return false;
  }
  out->encoded_message = out->owned_encoded_message;
  return true;
}

}  // namespace mojo
