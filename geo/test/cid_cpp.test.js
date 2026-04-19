import test from 'node:test';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';

test('C++ CID Consistency (Native)', (t) => {
  const testDir = path.resolve('geo/test');
  const cppSrc = path.join(testDir, 'cid_consistency_test.cpp');
  const cppBin = path.join(testDir, 'cid_consistency_test');
  const vendorDir = path.resolve('fs/cpp/vendor');

  // 1. Compile if needed (or always to be safe in tests)
  console.log('[C++ Test] Compiling CID consistency test...');
  const compile = spawnSync(
    'g++',
    ['-O3', '-std=c++17', cppSrc, '-I' + vendorDir, '-o', cppBin],
    { stdio: 'inherit' }
  );

  if (compile.status !== 0) {
    throw new Error('Failed to compile C++ CID consistency test');
  }

  // 2. Run the binary
  console.log('[C++ Test] Running CID consistency test...');
  const run = spawnSync(cppBin, [], { stdio: 'inherit' });

  assert.strictEqual(
    run.status,
    0,
    'C++ CID consistency test failed (see output above)'
  );
});
