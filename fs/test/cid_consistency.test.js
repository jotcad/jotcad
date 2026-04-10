import { test } from 'node:test';
import assert from 'node:assert';
import { VFS, getCID } from '../src/vfs_node.js';
import { registerVFSRoutes } from '../src/vfs_rest_server.js';
import http from 'node:http';

test('CID Consistency (Local JS vs Hub)', async (t) => {
  // 1. Setup VFS Hub
  const hubVfs = new VFS({ id: 'hub' });
  const server = http.createServer();
  registerVFSRoutes(hubVfs, server, '/vfs');

  const port = await new Promise((resolve) =>
    server.listen(0, '127.0.0.1', () => resolve(server.address().port))
  );
  const baseUrl = `http://127.0.0.1:${port}/vfs`;

  const testCases = [
    {
        name: 'Simple path with flat parameters',
        path: 'shape/hexagon',
        parameters: { radius: 50, variant: 'full' }
    },
    {
        name: 'Unordered parameters (should normalize)',
        path: 'shape/box',
        parameters: { height: 20, width: 10 }
    },
    {
        name: 'Deeply nested structured selectors',
        path: 'op/export',
        parameters: {
            filename: 'test.pdf',
            source: {
                path: 'op/pdf',
                parameters: {
                    source: {
                        path: 'op/outline',
                        parameters: {
                            source: {
                                path: 'op/offset',
                                parameters: {
                                    kerf: 5,
                                    source: {
                                        path: 'shape/hexagon',
                                        parameters: { radius: 50, variant: 'full' }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
  ];

  for (const tc of testCases) {
    await t.test(tc.name, async () => {
        // A. Local JS Calculation
        const localCid = await getCID({ path: tc.path, parameters: tc.parameters });
        
        // B. Hub-side Calculation (via endpoint)
        const response = await fetch(`${baseUrl}/cid`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ path: tc.path, parameters: tc.parameters })
        });
        const { cid: hubCid } = await response.json();

        console.log(`[Test] ${tc.name}`);
        console.log(`   CID: ${localCid}`);
        
        assert.strictEqual(localCid, hubCid, `Hub CID should match local JS CID for: ${tc.name}`);
    });
  }

  // Final check: Parameters with different key order but same content
  await t.test('Parameter Key Order Normalization', async () => {
    const cid1 = await getCID({ path: 'test', parameters: { a: 1, b: 2 } });
    const cid2 = await getCID({ path: 'test', parameters: { b: 2, a: 1 } });
    assert.strictEqual(cid1, cid2, 'CID should be identical regardless of parameter key order');
  });

  // Cleanup
  server.close();
  await hubVfs.close();
});
