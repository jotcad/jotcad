import { describe, it } from 'node:test';
import { testPng, withTestSession } from './test_session_util.js';
import { Orb } from './orb.js';
import { Point } from './point.js';
import { Relief } from './relief.js';
import assert from 'node:assert/strict';
import { png } from './png.js';
import { run } from '@jotcad/op';
import { z } from './z.js';

describe('relief op coverage', () => {
  it('Planar Mapping - Sine Wave', async () => {
    await withTestSession('relief_planar', async (session) => {
      await run(session, () => {
        const points = [];
        for (let x = -10; x <= 10; x += 1) {
          for (let y = -10; y <= 10; y += 1) {
            const z = Math.sin(Math.sqrt(x * x + y * y));
            points.push(Point(x, y, z));
          }
        }
        return Relief(points, {
          rows: 20,
          cols: 20,
          mapping: 'planar',
          subdivisions: 4,
        }).png('observed.relief_planar.png', [40, 40, 40], { wireframe: true });
      });
      assert.ok(
        await testPng(
          session,
          'relief_planar.png',
          'observed.relief_planar.png'
        )
      );
    });
  });

  it('Spherical Mapping - Noisy Orb', async () => {
    await withTestSession('relief_spherical', async (session) => {
      await run(session, () => {
        const points = [];
        const radius = 10;
        for (let phi = 0; phi < Math.PI; phi += 0.2) {
          for (let theta = 0; theta < 2 * Math.PI; theta += 0.2) {
            const r = radius + Math.sin(phi * 10) * Math.cos(theta * 10);
            const x = r * Math.sin(phi) * Math.cos(theta);
            const y = r * Math.sin(phi) * Math.sin(theta);
            const z = r * Math.cos(phi);
            points.push(Point(x, y, z));
          }
        }
        return Relief(points, {
          rows: 20,
          cols: 20,
          mapping: 'spherical',
          subdivisions: 4,
          closedU: true,
        }).png('observed.relief_spherical.png', [0, 0, 40], {
          wireframe: true,
        });
      });
      assert.ok(
        await testPng(
          session,
          'relief_spherical.png',
          'observed.relief_spherical.png'
        )
      );
    });
  });

  it('Cylindrical Mapping - Bumpy Tube', async () => {
    await withTestSession('relief_cylindrical', async (session) => {
      await run(session, () => {
        const points = [];
        const radius = 5;
        for (let z = -10; z <= 10; z += 1) {
          for (let theta = 0; theta < 2 * Math.PI; theta += 0.3) {
            const r = radius + Math.sin(z) * 0.5;
            const x = r * Math.cos(theta);
            const y = r * Math.sin(theta);
            points.push(Point(x, y, z));
          }
        }
        return Relief(points, {
          rows: 20,
          cols: 20,
          mapping: 'cylindrical',
          subdivisions: 4,
          closedU: true,
        })
          .z(-5)
          .png('observed.relief_cylindrical.png', [5, 0, 25], {
            wireframe: true,
          });
      });
      assert.ok(
        await testPng(
          session,
          'relief_cylindrical.png',
          'observed.relief_cylindrical.png'
        )
      );
    });
  });

  it('Isotropic Remeshing - Planar Sine', async () => {
    await withTestSession('relief_isotropic', async (session) => {
      await run(session, () => {
        const points = [];
        for (let x = -10; x <= 10; x += 1) {
          for (let y = -10; y <= 10; y += 1) {
            const z = Math.sin(Math.sqrt(x * x + y * y));
            points.push(Point(x, y, z));
          }
        }
        return Relief(points, {
          rows: 20,
          cols: 20,
          mapping: 'planar',
          subdivisions: 4,
          targetEdgeLength: 1.0,
        }).png('observed.relief_isotropic.png', [40, 40, 40], {
          wireframe: true,
        });
      });
      assert.ok(
        await testPng(
          session,
          'relief_isotropic.png',
          'observed.relief_isotropic.png'
        )
      );
    });
  });

  it('Isotropic Remeshing - Spherical Orb', async () => {
    await withTestSession('relief_isotropic_spherical', async (session) => {
      await run(session, () => {
        const points = [];
        const radius = 10;
        for (let phi = 0; phi < Math.PI; phi += 0.2) {
          for (let theta = 0; theta < 2 * Math.PI; theta += 0.2) {
            const r = radius + Math.sin(phi * 10) * Math.cos(theta * 10);
            const x = r * Math.sin(phi) * Math.cos(theta);
            const y = r * Math.sin(phi) * Math.sin(theta);
            const z = r * Math.cos(phi);
            points.push(Point(x, y, z));
          }
        }
        return Relief(points, {
          rows: 20,
          cols: 20,
          mapping: 'spherical',
          subdivisions: 4,
          closedU: true,
          targetEdgeLength: 1.0,
        }).png('observed.relief_isotropic_spherical.png', [30, 30, 30], {
          wireframe: true,
        });
      });
      assert.ok(
        await testPng(
          session,
          'relief_isotropic_spherical.png',
          'observed.relief_isotropic_spherical.png'
        )
      );
    });
  });
});
