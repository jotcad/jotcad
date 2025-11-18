import { Shape, makeShape } from './shape.js';
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

describe('Shape', () => {
  it('should create a shape instance', () => {
    const shape = new Shape();
    assert.ok(shape instanceof Shape);
  });

  it('should create a shape with geometry', () => {
    const shape = makeShape({ geometry: 'test_geometry_id' });
    assert.strictEqual(shape.geometry, 'test_geometry_id');
  });

  it('should derive a new shape', () => {
    const originalShape = makeShape({ geometry: 'original' });
    const derivedShape = originalShape.derive({ geometry: 'derived' });
    assert.strictEqual(derivedShape.geometry, 'derived');
    assert.ok(derivedShape instanceof Shape);
  });
});
