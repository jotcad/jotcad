import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import path from 'node:path';

runIntegrationTest('C++ CID Consistency (Native)', async ({ t }) => {
  const __dirname = path.dirname(new URL(import.meta.url).pathname);
  const cppBin = path.resolve(__dirname, '../geo/test/bin/cid_consistency_test');

  // 1. Run the binary (Assumes built by build.sh or make)
  console.log('[C++ Test] Running CID consistency test...');
  const run = spawnSync(cppBin, [], { stdio: 'inherit' });

  assert.strictEqual(
    run.status,
    0,
    'C++ CID consistency test failed (see output above). Run ./build.sh to rebuild.'
  );
});
