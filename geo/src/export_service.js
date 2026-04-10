import { VFS, RESTBridge } from '../../fs/src/index.js';
import fs from 'node:fs/promises';
import path from 'node:path';

const vfs = new VFS({ id: 'export-service' });
const bridge = new RESTBridge(vfs, 'http://localhost:9090/vfs');

async function main() {
    console.log('[Export Service] Initializing...');
    await bridge.start();

    // 1. Declare Schema
    vfs.declare('op/export', {
        type: 'object',
        properties: {
            source: { type: 'string' },
            filename: { type: 'string', default: 'download.bin' }
        }
    });

    // 2. Advertise LISTENING
    await vfs.receive({
        path: 'op/export',
        parameters: {},
        state: 'LISTENING',
        source: 'peer:export-service'
    });

    console.log('[Export Service] Listening for op/export requests...');

    // 3. Watch for PENDING requests on op/export
    const query = vfs.watch('op/export', { states: ['PENDING'] });
    for await (const req of query) {
        console.log(`[Export Service] Exporting ${req.path}...`);
        try {
            const { source, filename = 'download.bin' } = req.parameters;
            if (!source) throw new Error('No source provided for export');

            // Parse source selector
            let srcPath, srcParams;
            if (typeof source === 'object') {
                srcPath = source.path;
                srcParams = source.parameters || {};
            } else {
                // Fallback for string URIs
                srcPath = source;
                srcParams = {};
                if (srcPath.startsWith('vfs:/')) srcPath = srcPath.slice(5);
                const q = srcPath.indexOf('?');
                if (q !== -1) {
                    const queryStr = srcPath.slice(q + 1);
                    srcPath = srcPath.slice(0, q);
                    try { srcParams = JSON.parse(decodeURIComponent(queryStr)); } catch(e) {}
                }
            }

            // Read source data as raw bytes
            const stream = await vfs.read(srcPath, srcParams);
            const chunks = [];
            const reader = stream.getReader();
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                chunks.push(value);
            }
            const data = Buffer.concat(chunks.map(c => Buffer.from(c)));

            // Write to local filesystem (relative to the project root)
            const outPath = path.resolve(filename);
            await fs.writeFile(outPath, data);
            console.log(`[Export Service] Successfully exported to ${outPath}`);

            // Write a confirmation/metadata object back to VFS to fulfill the request
            const status = { 
                exportedAt: new Date().toISOString(), 
                filename: outPath, 
                size: data.length 
            };
            await vfs.writeData(req.path, req.parameters, status);

        } catch (err) {
            console.error(`[Export Service] Error fulfilling ${req.path}:`, err.message);
        }
    }
}

main().catch(err => {
    console.error('[Export Service] Fatal Error:', err);
    process.exit(1);
});
