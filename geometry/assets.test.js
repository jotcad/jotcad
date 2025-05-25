import { describe, it } from 'node:test';

import assert from 'node:assert/strict';
import { withAssets } from './assets.js';

describe('assets', () =>
  it('should create an asset textId', () =>
    withAssets((assets) => {
      const id = assets.textId('hello world');
      assert.strictEqual(
        'b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9',
        id
      );
      const value = assets.getText(id);
      assert.strictEqual('hello world', value);
    })));
