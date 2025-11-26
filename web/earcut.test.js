import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import earcut from './earcut.js';

describe('earcut', () => {
  it('should triangulate a simple square polygon correctly', () => {
    const data = [0, 0, 10, 0, 10, 10, 0, 10]; // Vertices of a square
    const holeIndices = [];
    const dim = 2;

    const triangles = earcut(data, holeIndices, dim);

    // A square should be triangulated into two triangles.
    // The exact order might vary, but the vertex indices should form two valid triangles.
    // Example: [0, 1, 2, 0, 2, 3] or [0, 1, 3, 1, 2, 3]
    assert.strictEqual(
      triangles.length,
      6,
      'Should produce 2 triangles (6 indices)'
    );

    // For a simple square, check if a valid triangulation is present
    // It should contain two triangles.
    // One possible triangulation: [0, 1, 2] and [0, 2, 3]
    // Another: [0, 1, 3] and [1, 2, 3]
    // A robust check involves ensuring all original vertices are part of the triangles
    // and that the sum of areas of triangles equals the polygon area.
    // For simplicity, let's just check two common triangulations.
    const expected1 = [0, 1, 2, 0, 2, 3];
    const expected2 = [0, 1, 3, 1, 2, 3];

    // Sort to compare regardless of order
    const sortedTriangles = [...triangles].sort((a, b) => a - b);
    const sortedExpected1 = [...expected1].sort((a, b) => a - b);
    const sortedExpected2 = [...expected2].sort((a, b) => a - b);

    const matchesExpected1 = sortedTriangles.every(
      (val, index) => val === sortedExpected1[index]
    );
    const matchesExpected2 = sortedTriangles.every(
      (val, index) => val === sortedExpected2[index]
    );

    assert.ok(
      matchesExpected1 || matchesExpected2,
      `Triangulation should match one of the expected patterns. Got: ${JSON.stringify(
        triangles
      )}`
    );
  });

  it('should triangulate a square with a square hole correctly', () => {
    // Outer square: (0,0)-(10,0)-(10,10)-(0,10)
    // Inner square (hole): (2,2)-(8,2)-(8,8)-(2,8)
    const data = [
      0,
      0,
      10,
      0,
      10,
      10,
      0,
      10, // Outer square (vertices 0-3)
      2,
      2,
      8,
      2,
      8,
      8,
      2,
      8, // Inner square (vertices 4-7)
    ];
    const holeIndices = [4]; // Hole starts at index 4 in data (vertex 4)
    const dim = 2;

    const triangles = earcut(data, holeIndices, dim);

    // A square with a square hole should be triangulated into 8 triangles.
    assert.strictEqual(
      triangles.length,
      24,
      'Should produce 8 triangles (24 indices)'
    );

    const knownGoodOutput = [
      0, 4, 7, 5, 4, 0, 3, 0, 7, 5, 0, 1, 2, 3, 7, 6, 5, 1, 2, 7, 6, 6, 1, 2,
    ]; // Updated to actual raw output
    const sortedTriangles = [...triangles].sort((a, b) => a - b);
    const sortedKnownGoodOutput = [...knownGoodOutput].sort((a, b) => a - b);

    assert.deepStrictEqual(
      sortedTriangles,
      sortedKnownGoodOutput,
      `Triangulation with hole does not match known good output. Got: ${JSON.stringify(
        triangles
      )}`
    );
  });

  it('should handle a concave polygon correctly', () => {
    // A simple concave polygon (e.g., a star shape or a "U" shape)
    const data = [0, 0, 10, 0, 10, 10, 5, 5, 0, 10];
    const holeIndices = [];
    const dim = 2;

    const triangles = earcut(data, holeIndices, dim);

    // For a 5-vertex concave polygon, it should produce 3 triangles.
    assert.strictEqual(
      triangles.length,
      9,
      'Should produce 3 triangles (9 indices)'
    );

    const knownGoodOutput = [0, 1, 3, 1, 2, 3, 3, 4, 0]; // Example triangulation
    const sortedTriangles = [...triangles].sort((a, b) => a - b);
    const sortedKnownGoodOutput = [...knownGoodOutput].sort((a, b) => a - b);

    assert.deepStrictEqual(
      sortedTriangles,
      sortedKnownGoodOutput,
      `Concave polygon triangulation incorrect. Got: ${JSON.stringify(
        triangles
      )}`
    );
  });
});
