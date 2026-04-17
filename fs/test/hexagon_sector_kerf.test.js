import test from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage } from '../src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';

const PORT_OPS = 9901;
const PORT_CLIENT = 9902;
const STORAGE_JS = path.resolve('.vfs_storage_sector_test');

test('Complex Mesh Expression: Hexagon Sector with Kerf', async (t) => {
  let opsProcess, clientServer, clientVfs, stopServer;

  t.after(async () => {
    console.log('[Test Sector] Cleaning up...');
    if (opsProcess) opsProcess.kill();
    if (stopServer) stopServer();
    if (clientServer) clientServer.close();
    if (clientVfs) await clientVfs.close();
    await fs.rm(STORAGE_JS, { recursive: true, force: true }).catch(() => {});
    await fs
      .rm(path.resolve('.vfs_storage_sector-ops'), {
        recursive: true,
        force: true,
      })
      .catch(() => {});
  });

  // 1. Start C++ Ops Node
  console.log('[Test Sector] Launching C++ Ops Node...');
  opsProcess = spawn(path.resolve('geo/bin/ops'), [], {
    env: { ...process.env, PORT: PORT_OPS.toString(), PEER_ID: 'sector-ops' },
  });

  await new Promise((resolve) => setTimeout(resolve, 2000));

  // 2. Start JS Client Node
  console.log('[Test Sector] Starting JS Client...');
  clientVfs = new VFS({
    id: 'sector-client',
    storage: new DiskStorage(STORAGE_JS),
  });
  const mesh = new MeshLink(clientVfs, [`http://localhost:${PORT_OPS}`], {
    localUrl: `http://localhost:${PORT_CLIENT}`,
  });
  clientServer = http.createServer();
  stopServer = registerVFSRoutes(clientVfs, clientServer, '', mesh);
  await new Promise((resolve) =>
    clientServer.listen(PORT_CLIENT, '0.0.0.0', resolve)
  );
  await clientVfs.init();
  await mesh.start();

  await t.test(
    'should resolve nested mesh expression for kerfed sector',
    async () => {
      // THE COMPLEX SELECTOR
      // pdf(offset(loop(group(origin, nth(points(hexagon), [0, 1]))), kerf=5))

      const expression = {
        path: 'op/pdf',
        parameters: {
          source: {
            path: 'op/offset',
            parameters: {
              radius: 5.0, // The Kerf
              source: {
                path: 'op/loop',
                parameters: {
                  source: {
                    path: 'op/group',
                    parameters: {
                      sources: [
                        { path: 'shape/origin', parameters: {} },
                        {
                          path: 'op/nth',
                          parameters: {
                            indices: [0, 1],
                            source: {
                              path: 'op/points',
                              parameters: {
                                source: {
                                  path: 'shape/hexagon',
                                  parameters: { radius: 100 },
                                },
                              },
                            },
                          },
                        },
                      ],
                    },
                  },
                },
              },
            },
          },
        },
      };

      console.log('[Test Sector] Requesting complex expression...');
      const pdfData = await clientVfs.readData(
        expression.path,
        expression.parameters
      );

      assert.ok(pdfData, 'Should return PDF data');
      console.log(
        `[Test Sector] Received data type: ${pdfData.constructor.name}, length: ${pdfData.length}`
      );

      // pdfData should be a Uint8Array
      const header = new TextDecoder().decode(pdfData.slice(0, 4));
      assert.strictEqual(header, '%PDF', 'Result should be a valid PDF buffer');

      console.log(
        `[Test Sector] SUCCESS: Generated ${pdfData.length} byte kerfed sector PDF.`
      );
    }
  );
});
