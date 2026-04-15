import test from 'node:test';
import assert from 'node:assert';
import { getCID } from '../src/vfs_node.js';

test('JS CID Reference Values', async (t) => {
  const cases = [
    {
      name: 'empty',
      path: 'geo/test',
      parameters: {},
      cid: 'cb932f0d598aaad75c101df6376df477c895388805bf90044727a712fb8bf2e7',
    },
    {
      name: 'integers',
      path: 'geo/box',
      parameters: { width: 10, height: 20 },
      cid: '720a69d8ab575e9cc7239e7a29ad0ab599ba1e2900a72780d27ff244de4e3fcf',
    },
    {
      name: 'floats',
      path: 'geo/circle',
      parameters: { diameter: 10.5 },
      cid: '6c533b232b5fbed3c7c8ad10d61dca6f6975292a5db58b7e3331bd04c8a3d48f',
    },
    {
      name: 'nested',
      path: 'geo/offset',
      parameters: {
        kerf: 1,
        source: { path: 'geo/box', parameters: { width: 5 } },
      },
      cid: '6ac24561eef57aaf8d9924d6caac17edaa8178d6fa4998c09ac8bd495685706b',
    },
    {
      name: 'integers as floats',
      path: 'geo/box',
      parameters: { width: 10.0 },
      cid: '47de0d7742a69477cec99b5e64a0783a8c1e51efeab9cf510104b2c55831b293',
    },
  ];

  for (const tc of cases) {
    await t.test(tc.name, async () => {
      const actual = await getCID({ path: tc.path, parameters: tc.parameters });
      assert.strictEqual(actual, tc.cid, `CID mismatch for ${tc.name}`);
    });
  }
});
