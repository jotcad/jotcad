import { Orb } from './orb.js';
import { withAssets } from './assets.js';
import { run } from '@jotcad/op';
import { writeFileSync } from 'node:fs';

test('Orb', async () => {
  await withAssets('/tmp/test-orb', async (assets) => {
    const geometry = Orb(assets, 30, 0.1, 0.1);
    expect(geometry).toBeDefined();
    // You might want to add more specific assertions here,
    // e.g., checking the type of geometry or some properties.
  });
});
