import test from 'node:test';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';

test('C++ CID Consistency (Native)', (t) => {
  const __dirname = path.dirname(new URL(import.meta.url).pathname);
  const cppBin = path.resolve(__dirname, 'bin/cid_consistency_test');

  // 1. Run the binary (Assumes built by build.sh or make)
  console.log('[C++ Test] Running CID consistency test...');
  const run = spawnSync(cppBin, [], { stdio: 'inherit' });

  assert.strictEqual(
    run.status,
    0,
    'C++ CID consistency test failed (see output above). Run ./build.sh to rebuild.'
  );
});
