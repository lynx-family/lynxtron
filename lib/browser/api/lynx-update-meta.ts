// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import LynxTemplateData from './lynx-template-data';

type LynxUpdateMetaInit = {
  updateData: LynxTemplateData;
  globalProps: LynxTemplateData;
};

export class LynxUpdateMeta {
  updateData: LynxTemplateData;
  globalProps: LynxTemplateData;

  constructor(init?: Partial<LynxUpdateMetaInit>) {
    this.updateData = init?.updateData ?? new LynxTemplateData({});
    this.globalProps = init?.globalProps ?? new LynxTemplateData({});
  }
}

export default LynxUpdateMeta;
