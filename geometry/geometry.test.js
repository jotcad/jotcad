import {
  DecodeInexactGeometryText,
  EncodeInexactGeometryText,
} from './geometry.js';

import { describe, it } from 'node:test';

import assert from 'node:assert/strict';

describe('geometry', () => {
  it('should encode faces', () => {
    assert.strictEqual(
      'v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nf 0 1 2\n',
      EncodeInexactGeometryText({
        vertices: [
          [0, 0, 0],
          [1, 0, 0],
          [0, 1, 0],
        ],
        segments: [],
        triangles: [],
        faces: [[[0, 1, 2]]],
      })
    );
  });

  it('should decode faces', () => {
    assert.deepEqual(
      {
        vertices: [
          [0, 0, 0],
          [1, 0, 0],
          [0, 1, 0],
        ],
        segments: [],
        triangles: [],
        faces: [[[0, 1, 2]]],
      },
      DecodeInexactGeometryText(
        'v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nf 0 1 2\n'
      )
    );
  });

  it('should encode triangles', () => {
    assert.strictEqual(
      'v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nt 0 1 2\n',
      EncodeInexactGeometryText({
        vertices: [
          [0, 0, 0],
          [1, 0, 0],
          [0, 1, 0],
        ],
        segments: [],
        triangles: [[0, 1, 2]],
        faces: [],
      })
    );
  });

  it('should decode triangles', () => {
    assert.deepEqual(
      {
        vertices: [
          [0, 0, 0],
          [1, 0, 0],
          [0, 1, 0],
        ],
        segments: [],
        triangles: [[0, 1, 2]],
        faces: [],
      },
      DecodeInexactGeometryText(
        'v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nt 0 1 2\n'
      )
    );
  });

  it('should encode segments', () => {
    assert.strictEqual(
      'v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\ns 0 1 1 2 2 0\n',
      EncodeInexactGeometryText({
        vertices: [
          [0, 0, 0],
          [1, 0, 0],
          [0, 1, 0],
        ],
        segments: [
          [0, 1],
          [1, 2],
          [2, 0],
        ],
        triangles: [],
        faces: [],
      })
    );
  });

  it('should decode segments', () => {
    assert.deepEqual(
      {
        vertices: [
          [0, 0, 0],
          [1, 0, 0],
          [0, 1, 0],
        ],
        segments: [
          [0, 1],
          [1, 2],
          [2, 0],
        ],
        triangles: [],
        faces: [],
      },
      DecodeInexactGeometryText(
        'v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\ns 0 1 1 2 2 0\n'
      )
    );
  });
});
