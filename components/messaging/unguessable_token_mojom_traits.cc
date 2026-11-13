// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.
#include "components/messaging/public/cpp/unguessable_token_mojom_traits.h"

namespace mojo {

bool StructTraits<messaging::mojom::UnguessableTokenDataView,
                  base::UnguessableToken>::
    Read(messaging::mojom::UnguessableTokenDataView data,
         base::UnguessableToken* output) {
  auto token = base::UnguessableToken::Deserialize(data.high(), data.low());
  if (!token) {
    return false;
  }
  *output = *token;
  return true;
}

}  // namespace mojo
