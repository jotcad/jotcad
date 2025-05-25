import { describe, it } from 'node:test';

import assert from 'node:assert/strict';
import { Box3 } from './box.js';
import { makeAbsolute } from './makeAbsolute.js';
import { withAssets } from './assets.js';

describe('makeAbsolute', () =>
  it('should make a box absolute', () =>
    withAssets(async (assets) => {
      const box = Box3(assets, [1, 3], [1, 3], [1, 3]);
      const absoluteBox = makeAbsolute(assets, box);
      assert.strictEqual(
        `V 8
v 1 1 1 1 1 1
v 3 1 1 3 1 1
v 1 3 1 1 3 1
v 3 3 1 3 3 1
v 1 1 3 1 1 3
v 3 1 3 3 1 3
v 1 3 3 1 3 3
v 3 3 3 3 3 3
T 12
t 7 3 2
t 2 6 7
t 7 6 4
t 4 5 7
t 7 5 1
t 1 3 7
t 0 2 3
t 3 1 0
t 0 1 5
t 5 4 0
t 6 2 0
t 0 4 6
`,
        assets.getText(absoluteBox.geometry)
      );
    })));
