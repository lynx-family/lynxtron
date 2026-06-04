import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { PLATFROM_EXE_PATH } from './utils/env-config.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export default path.join(__dirname, "dist", PLATFROM_EXE_PATH);

// export default "dist/" + getPlatformPath();