import path from 'node:path';
import { fileURLToPath } from 'node:url';
import fs from 'node:fs';

import { UGCSession, startSessionServer } from '../ugc/src/index.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

async function main() {
  const port = 3051;
  // Allow passing the session folder dynamically, defaulting to the workspace scratch session directory
  const sessionArg = process.argv[2];
  const sessionDir = sessionArg
    ? path.resolve(sessionArg)
    : path.resolve(__dirname, '../scratch/my_cad_session');

  if (!fs.existsSync(sessionDir)) {
    fs.mkdirSync(sessionDir, { recursive: true });
  }

  // Display-only session reader (requires no compiler/evaluator engine)
  const session = new UGCSession(sessionDir, null);

  console.log(`[cli-viewer] Serving session folder: ${sessionDir}`);
  startSessionServer(port, session, '0.0.0.0');
  console.log(`[cli-viewer] Server listening on https://localhost:${port}`);
}

main().catch(err => {
  console.error('[cli-viewer] Failed to start:', err);
  process.exit(1);
});
