import { describe, it } from 'node:test';
import { strict as assert } from 'node:assert';
import { withTestAssets } from './assets.js';
import { Box3 } from './box.js';
import { footprint } from './footprint.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';

describe('footprint', () => {
  it(
    'should create a 2D footprint from a 3D box',
    withTestAssets(async (assets) => {
      const box = Box3(10, 10, 10); // A 10x10x10 cube
      const footprintShape = footprint(box);

      // Verify that a valid geometry was returned
      assert.ok(
        footprintShape.geometryId !== undefined &&
          footprintShape.geometryId !== null,
        'Footprint should return a valid geometry ID'
      );

      // The footprint of a 10x10x10 box should be a 10x10 square.
      // This implies 1 face (the square) and 4 vertices.

      // Get the geometry from assets to inspect it
      const footprintGeometry = assets.Get(footprintShape.geometryId);
      assert.equal(
        footprintGeometry.vertices_.length,
        4,
        'Footprint should have 4 vertices'
      );
      assert.equal(
        footprintGeometry.faces_.length,
        1,
        'Footprint should have 1 face'
      );
      assert.equal(
        footprintGeometry.faces_[0].first.length,
        4,
        'The face should have 4 vertices'
      );
      assert.equal(
        footprintGeometry.faces_[0].second.length,
        0,
        'The face should have no holes'
      );

      // Test the visual output using renderPng and testPng
      // We expect a square outline.
      const outputPath = assets.getPath('footprint_box.png');
      await renderPng(footprintShape, outputPath, { width: 100, height: 100 });
      // This will compare the generated PNG with a reference PNG (if available)
      // or generate a new reference PNG if none exists.
      // The reference image should depict a 10x10 square.
      await testPng(outputPath, assets.getPath('footprint_box.png'));
    })
  );

  it(
    'should create a 2D footprint from an object with a hole',
    withTestAssets(async (assets) => {
      // Create an object with a hole. For simplicity, let's represent a square
      // with a square hole in the middle using direct geometry manipulation.
      // This is more complex than a simple Box3, but demonstrates hole handling.

      // Outer square (20x20)
      const outerSquareVertices = [
        { x: -10, y: -10, z: 0 },
        { x: 10, y: -10, z: 0 },
        { x: 10, y: 10, z: 0 },
        { x: -10, y: 10, z: 0 },
      ];
      const outerSquareTriangles = [
        [0, 1, 2],
        [0, 2, 3],
      ];

      // Inner square (5x5) - this will be a hole when projected
      const innerSquareVertices = [
        { x: -2.5, y: -2.5, z: 0 },
        { x: 2.5, y: -2.5, z: 0 },
        { x: 2.5, y: 2.5, z: 0 },
        { x: -2.5, y: 2.5, z: 0 },
      ];
      const innerSquareTriangles = [
        [4, 5, 6], // These indices are relative to the combined geometry
        [4, 6, 7],
      ];

      // Build a custom geometry that resembles a flat plate with a hole.
      // We need to construct a geometry with triangles that represent this.
      // For simplicity, let's just make two separate boxes and then rely on
      // boolean operations in footprint to union them. This isn't directly creating a hole *in 3D*
      // but rather two overlapping 3D objects whose 2D projection creates a hole.
      // A better approach would be to construct the input geometry from scratch
      // or use a more complex solid modeling operation if available.

      // Let's create two boxes, one inside another, and use the difference operation
      // if available, or simulate it. For now, let's assume footprint can derive holes
      // from overlapping geometry that forms a void.

      // Create a larger box and a smaller box that's "removed" from the larger one.
      const largeBox = Box3(20, 20, 1); // A thin plate
      const smallBox = Box3(5, 5, 1); // A thin plate in the middle

      // To simulate a hole, we'd typically use a boolean difference operation.
      // Assuming `footprint` works correctly with complex geometries and can identify
      // holes from the union of triangles (which CGAL::Polygon_set_2 does).
      // Let's create a donut-like shape for the 3D object to ensure a hole in the footprint.
      // This requires more than just Box3. For a proper hole test, I'd need to either
      // manually construct the mesh for a donut or use a boolean difference.

      // For now, let's create a simplified test: two rectangles that, when projected, result in a hole.
      // This test will be more about CGAL's Polygon_set_2 union behavior.

      // A simple hollow square using triangles.
      // Vertices for a large square (say, 0,0 to 10,10)
      // and a smaller square inside (say, 2,2 to 8,8).
      // The triangles would define the area between these two squares.

      const customGeometryId = assets.addEmpty();
      const customGeometry = assets.Get(customGeometryId);

      // Outer square vertices
      const v0 = customGeometry.AddVertex(assets.Point3(-10, -10, 0));
      const v1 = customGeometry.AddVertex(assets.Point3(10, -10, 0));
      const v2 = customGeometry.AddVertex(assets.Point3(10, 10, 0));
      const v3 = customGeometry.AddVertex(assets.Point3(-10, 10, 0));

      // Inner square vertices (for the hole)
      const v4 = customGeometry.AddVertex(assets.Point3(-5, -5, 0));
      const v5 = customGeometry.AddVertex(assets.Point3(5, -5, 0));
      const v6 = customGeometry.AddVertex(assets.Point3(5, 5, 0));
      const v7 = customGeometry.AddVertex(assets.Point3(-5, 5, 0));

      // Triangles for the outer ring (connecting outer to inner)
      customGeometry.AddTriangle(v0, v1, v4);
      customGeometry.AddTriangle(v1, v5, v4);

      customGeometry.AddTriangle(v1, v2, v5);
      customGeometry.AddTriangle(v2, v6, v5);

      customGeometry.AddTriangle(v2, v3, v6);
      customGeometry.AddTriangle(v3, v7, v6);

      customGeometry.AddTriangle(v3, v0, v7);
      customGeometry.AddTriangle(v0, v4, v7);

      const hollowSquareShape = {
        geometryId: customGeometryId,
        tfId: assets.Id(assets.identity()),
      };

      const footprintShapeWithHole = footprint(hollowSquareShape);

      assert.ok(
        footprintShapeWithHole.geometryId !== undefined &&
          footprintShapeWithHole.geometryId !== null,
        'Footprint with hole should return a valid geometry ID'
      );

      const footprintGeometryWithHole = assets.Get(
        footprintShapeWithHole.geometryId
      );

      // Expect 8 vertices (4 outer, 4 inner)
      assert.equal(
        footprintGeometryWithHole.vertices_.length,
        8,
        'Footprint with hole should have 8 vertices'
      );
      // Expect 1 face with 1 hole
      assert.equal(
        footprintGeometryWithHole.faces_.length,
        1,
        'Footprint with hole should have 1 face'
      );
      assert.equal(
        footprintGeometryWithHole.faces_[0].first.length,
        4,
        'Outer boundary of face should have 4 vertices'
      );
      assert.equal(
        footprintGeometryWithHole.faces_[0].second.length,
        1,
        'Face should have 1 hole'
      );
      assert.equal(
        footprintGeometryWithHole.faces_[0].second[0].length,
        4,
        'The hole should have 4 vertices'
      );

      const outputPath = assets.getPath('footprint_hollow_square.png');
      await renderPng(footprintShapeWithHole, outputPath, {
        width: 100,
        height: 100,
      });
      await testPng(outputPath, assets.getPath('footprint_hollow_square.png'));
    })
  );
});
