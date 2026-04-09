// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import type LynxTemplateData from './lynx-template-data';

export class LynxUpdateMeta {
  constructor(init?: {
    updateData?: LynxTemplateData;
    globalProps?: LynxTemplateData;
  });
  updateData: LynxTemplateData;
  globalProps: LynxTemplateData;
}

export default LynxUpdateMeta;
