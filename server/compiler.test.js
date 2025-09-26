import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { compile } from './compiler.js';

describe('compile', () => {
  it('Simple valid function call', async () => {
    assert.strictEqual('Obj();', compile('Obj()', { functions: ['Obj'] }));
  });

  it('Simple valid function chained call', async () => {
    assert.strictEqual(
      'Foo().Bar();',
      compile('Foo().Bar()', { functions: ['Foo'], methods: ['Bar'] })
    );
  });
});
