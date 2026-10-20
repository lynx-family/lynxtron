// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#ifndef COMPONENTS_MESSAGING_PUBLIC_CPP_UNGUESSABLE_TOKEN_MOJOM_TRAITS_H_
#define COMPONENTS_MESSAGING_PUBLIC_CPP_UNGUESSABLE_TOKEN_MOJOM_TRAITS_H_

#include "base/unguessable_token.h"
#include "components/messaging/public/mojom/unguessable_token.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<messaging::mojom::UnguessableTokenDataView,
                    base::UnguessableToken> {
  static uint64_t high(const base::UnguessableToken& token) {
    return token.GetHighForSerialization();
  }

  static uint64_t low(const base::UnguessableToken& token) {
    return token.GetLowForSerialization();
  }

  static bool Read(messaging::mojom::UnguessableTokenDataView data,
                   base::UnguessableToken* output);
};

}  // namespace mojo

#endif  // COMPONENTS_MESSAGING_PUBLIC_CPP_UNGUESSABLE_TOKEN_MOJOM_TRAITS_H_
