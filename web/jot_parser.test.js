import { DecodeInexactGeometryText } from './jot_parser.js';
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

describe('DecodeInexactGeometryText', () => {
  it('should correctly decode vertices and triangles from JOT text', () => {
    const jotData = `
V 8
v 0 0 0 0 0 0
v 1 0 0 1 0 0
v 0 1 0 0 1 0
v 1 1 0 1 1 0
v 0 0 1 0 0 1
v 1 0 1 1 0 1
v 0 1 1 0 1 1
v 1 1 1 1 1 1
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
`;
    const { vertices, triangles } = DecodeInexactGeometryText(jotData);

    assert.strictEqual(vertices.length, 8, 'Should have 8 vertices');
    assert.deepStrictEqual(
      vertices[0],
      [0, 0, 0, 0, 0, 0],
      'First vertex incorrect'
    );
    assert.deepStrictEqual(
      vertices[7],
      [1, 1, 1, 1, 1, 1],
      'Last vertex incorrect'
    );

    assert.strictEqual(triangles.length, 12, 'Should have 12 triangles');
    assert.deepStrictEqual(triangles[0], [7, 3, 2], 'First triangle incorrect');
    assert.deepStrictEqual(triangles[11], [0, 4, 6], 'Last triangle incorrect');
  });

  it('should ignore empty lines and lines with unknown commands', () => {
    const jotData = `
V 8

v 0 0 0 0 0 0
# This is a comment

t 1 2 3
UNKNOWN_COMMAND test
`;
    const { vertices, triangles } = DecodeInexactGeometryText(jotData);
    assert.strictEqual(vertices.length, 1, 'Should have 1 vertex');
    assert.strictEqual(triangles.length, 1, 'Should have 1 triangle');
    assert.deepStrictEqual(vertices[0], [0, 0, 0, 0, 0, 0]);
    assert.deepStrictEqual(triangles[0], [1, 2, 3]);
  });

  it('should handle JOT data with faces (polygons with holes)', () => {
    const jotData = `
V 6
v 0 0 0 0 0 0
v 1 0 0 1 0 0
v 1 1 0 1 1 0
v 0 1 0 0 1 0
v 0.25 0.25 0 0.25 0.25 0
v 0.75 0.25 0 0.75 0.25 0
F 1
f 0 1 2 3 (4 5)
`;
    const { vertices, faces } = DecodeInexactGeometryText(jotData);
    assert.strictEqual(
      vertices.length,
      6,
      'Should have 6 vertices for faces test'
    );
    assert.strictEqual(faces.length, 1, 'Should have 1 face');
    assert.deepStrictEqual(
      faces[0],
      [
        [0, 1, 2, 3],
        [4, 5],
      ],
      'Face with hole incorrect'
    );
  });

  it('should handle JOT data with segments (lines)', () => {
    const jotData = `
V 2
v 0 0 0 0 0 0
v 1 1 1 1 1 1
S 1
s 0 1
`;
    const { vertices, segments } = DecodeInexactGeometryText(jotData);
    assert.strictEqual(
      vertices.length,
      2,
      'Should have 2 vertices for segments test'
    );
    assert.strictEqual(segments.length, 1, 'Should have 1 segment');
    assert.deepStrictEqual(segments[0], [0, 1], 'Segment incorrect');
  });
});
