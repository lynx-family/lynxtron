// @ts-nocheck
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const lynxtron = require('lynxtron');

export const contextBridge = lynxtron.contextBridge;
