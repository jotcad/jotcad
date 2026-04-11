import { VFS, RESTBridge } from '../../fs/src/index.js';
import fs from 'node:fs';
import fsPromises from 'node:fs/promises';
import path from 'node:path';

const vfs = new VFS({ id: process.env.PEER_ID || 'export-service' });
const hubUrl = process.env.VFS_HUB_URL || 'http://localhost:9090/vfs';
const bridge = new RESTBridge(vfs, hubUrl);

const logFile = path.resolve('export_service.log');
function log(msg) {
    const timestamp = new Date().toISOString();
    fs.appendFileSync(logFile, `[${timestamp}] ${msg}\n`);
    console.log(msg);
}

async function main() {
    log('[Export Service] Initializing...');
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

    log('[Export Service] Listening for op/export requests...');

    // 3. Watch for PENDING requests on op/export
    const query = vfs.watch('op/export', { states: ['PENDING'] });
    for await (const req of query) {
        log(`[Export Service] Exporting ${req.path}...`);
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
            log(`[Export Service] Requesting source bytes: ${srcPath}`);
            let data = await vfs.readData(srcPath, srcParams);
            
            if (data && typeof data.getReader === 'function') {
                log(`[Export Service] Data is a stream, consuming...`);
                const chunks = [];
                const reader = data.getReader();
                while (true) {
                    const { done, value } = await reader.read();
                    if (done) break;
                    chunks.push(value);
                }
                const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
                const bytes = new Uint8Array(totalLength);
                let offset = 0;
                for (const chunk of chunks) {
                    bytes.set(chunk, offset);
                    offset += chunk.length;
                }
                data = bytes;
            }

            log(`[Export Service] Received source data (${data instanceof Uint8Array ? data.length : 'NOT BYTES'} bytes)`);

            // Write to local filesystem (relative to the project root)
            const outPath = path.resolve(filename);
            await fsPromises.writeFile(outPath, data);
            log(`[Export Service] Successfully exported to ${outPath}`);

            // Write a confirmation/metadata object back to VFS to fulfill the request
            const status = { 
                exportedAt: new Date().toISOString(), 
                filename: outPath, 
                size: data.length 
            };
            await vfs.writeData(req.path, req.parameters, status);

        } catch (err) {
            log(`[Export Service ERROR] Error fulfilling ${req.path}: ${err.message}`);
        }
    }
}

main().catch(err => {
    log(`[Export Service FATAL ERROR] ${err.message}`);
    process.exit(1);
});
