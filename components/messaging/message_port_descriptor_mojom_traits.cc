// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "components/messaging/public/cpp/message_port_descriptor_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<messaging::mojom::MessagePortDescriptorDataView,
                  messaging::MessagePortDescriptor>::
    Read(messaging::mojom::MessagePortDescriptorDataView data,
         messaging::MessagePortDescriptor* output) {
  mojo::ScopedMessagePipeHandle handle = data.TakePipeHandle();
  uint64_t sequence_number = data.sequence_number();
  base::UnguessableToken id;
  if (!data.ReadId(&id)) {
    return false;
  }

  output->InitializeFromSerializedValues(std::move(handle), id,
                                         sequence_number);
  return true;
}

// static
mojo::ScopedMessagePipeHandle
StructTraits<messaging::mojom::MessagePortDescriptorDataView,
             messaging::MessagePortDescriptor>::
    pipe_handle(messaging::MessagePortDescriptor& input) {
  return input.TakeHandleForSerialization();
}

// static
base::UnguessableToken StructTraits<
    messaging::mojom::MessagePortDescriptorDataView,
    messaging::MessagePortDescriptor>::id(messaging::MessagePortDescriptor&
                                              input) {
  return input.TakeIdForSerialization();
}

// static
uint64_t StructTraits<messaging::mojom::MessagePortDescriptorDataView,
                      messaging::MessagePortDescriptor>::
    sequence_number(messaging::MessagePortDescriptor& input) {
  return input.TakeSequenceNumberForSerialization();
}

}  // namespace mojo
