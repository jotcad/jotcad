import {
  compose,
  makeIdentity,
  makeInverse,
  makeMove,
  makeRotateX,
  makeRotateY,
  makeRotateZ,
  makeScale,
} from './transform.js';
import { describe, it } from 'node:test';

import { Point } from './point.js';
import assert from 'node:assert/strict';
import { cgal } from './getCgal.js';
import { withTestAssets } from './test_session_util.js';

describe('transform', () => {
  it('should simplify rotations', () => {
    assert.strictEqual('x 0.5 0.5', makeRotateX(0.5));
    assert.strictEqual('y 0.5 0.5', makeRotateY(0.5));
    assert.strictEqual('z 0.5 0.5', makeRotateZ(0.5));
    assert.strictEqual('x 1/2 0.5', cgal.SimplifyTransform(makeRotateX(0.5)));
    assert.strictEqual('y 1/2 0.5', cgal.SimplifyTransform(makeRotateY(0.5)));
    assert.strictEqual('z 1/2 0.5', cgal.SimplifyTransform(makeRotateZ(0.5)));
  });

  it('should simplify a move', () => {
    assert.strictEqual('t 0.5 0.5 0.5 0.5 0.5 0.5', makeMove(0.5, 0.5, 0.5));
    assert.strictEqual(
      't 1/2 1/2 1/2 0.5 0.5 0.5',
      cgal.SimplifyTransform(makeMove(0.5, 0.5, 0.5))
    );
  });

  it('should simplify a scale', () => {
    assert.strictEqual('s 0.5 0.5 0.5 0.5 0.5 0.5', makeScale(0.5, 0.5, 0.5));
    assert.strictEqual(
      's 1/2 1/2 1/2 0.5 0.5 0.5',
      cgal.SimplifyTransform(makeScale(0.5, 0.5, 0.5))
    );
  });

  it('should perform an inversion', () => {
    assert.strictEqual('i', makeInverse());
    assert.strictEqual(
      'x 3/4 0.75',
      cgal.SimplifyTransform(compose(makeInverse(), makeRotateX('1/4')))
    );
  });

  it('should perform a composition', () => {
    // The rotations cancel out back to the identity matrix.
    assert.strictEqual(
      undefined,
      cgal.SimplifyTransform(compose(makeRotateX(0.5), makeRotateX(-0.5)))
    );
    // These two compose to a rotation along the third axis.
    assert.strictEqual(
      'z 1/2 0.5',
      cgal.SimplifyTransform(compose(makeRotateX(0.5), makeRotateY(0.5)))
    );
    // These produce a complex matrix.
    assert.strictEqual(
      cgal.SimplifyTransform(compose(makeRotateX(0.5), makeRotateY(0.25))),
      'm 0 0 -1 0 -1 0 -1 0 0 0 0 0 0 -0 -1 0 -1 -0 -1 -0 -0 0 -0 -0'
    );
  });

  it('should translate a point', async () => {
    await withTestAssets('should translate a point', async (assets) => {
      const transformedPoint = Point(assets, 1, 2, 3).move(3, 2, 1);
      const id = cgal.MakeAbsolute(assets, transformedPoint);
      assert.strictEqual('V 1\nv 4 4 4 4 4 4\np 0\n', assets.getText(id));
    });
  });

  it('should rotate points', async () => {
    await withTestAssets('should rotate points', async (assets) => {
      assert.strictEqual(
        'V 1\nv 0 -1 0 0 -1 0\np 0\n',
        assets.getText(
          cgal.MakeAbsolute(assets, Point(assets, 1, 0, 0).rotateZ(1 / 4))
        )
      );
      assert.strictEqual(
        'V 1\nv -1 0 0 -1 0 0\np 0\n',
        assets.getText(
          cgal.MakeAbsolute(assets, Point(assets, 1, 0, 0).rotateZ(1 / 2))
        )
      );
      assert.strictEqual(
        'V 1\nv -451/901 -780/901 0 -0.500555 -0.865705 0\np 0\n',
        assets.getText(
          cgal.MakeAbsolute(assets, Point(assets, 1, 0, 0).rotateZ(1 / 3))
        )
      );
    });
  });
});
