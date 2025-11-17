// Copyright (c) 2016 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef LYNXTRON_SHELL_APP_MAC_DICT_UTIL_H_
#define LYNXTRON_SHELL_APP_MAC_DICT_UTIL_H_

#import <Foundation/Foundation.h>

#include "base/values.h"

namespace lynxtron {

NSArray* ListValueToNSArray(const base::Value::List& value);
base::Value::List NSArrayToValue(NSArray* arr);
NSDictionary* DictionaryValueToNSDictionary(const base::Value::Dict& value);
base::Value::Dict NSDictionaryToValue(NSDictionary* dict);

}  // namespace lynxtron

#endif  // LYNXTRON_SHELL_APP_MAC_DICT_UTIL_H_
