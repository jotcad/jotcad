import { VFS, RESTBridge } from '../../fs/src/index.js';
import fs from 'node:fs/promises';
import path from 'node:path';

async function test() {
    console.log('[Test] Starting Pipeline Test (Structured)...');
    const vfs = new VFS({ id: 'test-runner' });
    const bridge = new RESTBridge(vfs, 'http://localhost:9090/vfs');
    await bridge.start();

    try {
        // Define the Pipeline using Structured Selectors
        const filename = 'pipeline_structured.pdf';
        
        const exportParams = {
            filename,
            source: {
                path: 'op/pdf',
                parameters: {
                    source: {
                        path: 'op/outline',
                        parameters: {
                            source: {
                                path: 'op/offset',
                                parameters: {
                                    kerf: 5.0,
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
        };

        console.log('[Test] Triggering full pipeline via structured op/export...');
        
        const result = await vfs.readData('op/export', exportParams);

        console.log('[Test] Pipeline completed successfully!');
        console.log('[Test] Export Metadata:', JSON.stringify(result, null, 2));

        const stats = await fs.stat(filename);
        console.log(`[Test] Verified: ${filename} exists (${stats.size} bytes)`);

    } catch (err) {
        console.error('[Test] Pipeline failed:', err);
        process.exit(1);
    } finally {
        await bridge.stop();
        vfs.close();
    }
}

test();
