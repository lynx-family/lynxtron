// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Intentionally no include guard. Xcode 27's <math.h> defines
// __need_infinity_nan and reincludes <float.h> to request only these macros.
#if defined(__APPLE__) && defined(CR_XCODE_VERSION) && \
    CR_XCODE_VERSION >= 2700 && defined(__need_infinity_nan)
#undef INFINITY
#undef NAN
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))
#undef __need_infinity_nan
#else
#include_next <float.h>
#endif
