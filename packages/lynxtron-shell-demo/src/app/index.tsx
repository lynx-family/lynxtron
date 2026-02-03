import '@lynx-js/preact-devtools';
import { root } from '@lynx-js/react';

import { App } from './App.tsx';

root.render(<App />);

if (import.meta.webpackHot) {
  import.meta.webpackHot.accept();
}
