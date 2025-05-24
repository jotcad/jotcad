import { cgal, cgalIsReady } from './getCgal.js';

import { existsSync } from 'node:fs';
import test from 'ava';
import { withAssets } from './assets.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('textId', (t) =>
  withAssets((assets) => {
    const id = assets.textId('hello world');
    t.is(
      'b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9',
      id
    );
    const value = assets.getText(id);
    t.is('hello world', value);
  }));
