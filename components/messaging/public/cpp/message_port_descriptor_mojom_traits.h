// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef COMPONENTS_MESSAGING_PUBLIC_CPP_MESSAGE_PORT_DESCRIPTOR_MOJOM_TRAITS_H_
#define COMPONENTS_MESSAGING_PUBLIC_CPP_MESSAGE_PORT_DESCRIPTOR_MOJOM_TRAITS_H_

#include "components/messaging/public/cpp/message_port_descriptor.h"
#include "components/messaging/public/cpp/unguessable_token_mojom_traits.h"
#include "components/messaging/public/mojom/message_port_descriptor.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<messaging::mojom::MessagePortDescriptorDataView,
                    messaging::MessagePortDescriptor> {
  // For converting a messaging::mojom::MessagePortDescriptor to a
  // messaging::MessagePortDescriptor.
  static bool Read(messaging::mojom::MessagePortDescriptorDataView data,
                   messaging::MessagePortDescriptor* output);

  // For converting a messaging::MessagePortDescriptor to a
  // messaging::mojom::MessagePortDescriptor.
  static mojo::ScopedMessagePipeHandle pipe_handle(
      messaging::MessagePortDescriptor& input);
  static base::UnguessableToken id(messaging::MessagePortDescriptor& input);
  static uint64_t sequence_number(messaging::MessagePortDescriptor& input);
};

}  // namespace mojo

#endif  // COMPONENTS_MESSAGING_PUBLIC_CPP_MESSAGE_PORT_DESCRIPTOR_MOJOM_TRAITS_H_
