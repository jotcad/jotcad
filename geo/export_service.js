import http from 'node:http';
import https from 'node:https';
import fs from 'node:fs';
import fsPromises from 'node:fs/promises';
import path from 'node:path';
import { VFS, DiskStorage, MeshLink, registerVFSRoutes } from '../fs/src/index.js';

const id = process.env.VFS_ID || 'export-node';
const port = parseInt(process.env.PORT || '9092');
const neighbors = (process.env.NEIGHBORS || '').split(',').filter(Boolean);
const storageDir = path.resolve(`.vfs_storage_${id}`);

const __dirname = path.resolve();
const sslDir = path.join(__dirname, '.ssl');
const keyPath = path.join(sslDir, 'localhost-key.pem');
const certPath = path.join(sslDir, 'localhost-cert.pem');

let server;
let protocol = 'http';

if (process.env.DISABLE_SSL !== '1' && fs.existsSync(keyPath) && fs.existsSync(certPath)) {
    console.log(`[Export Node ${id}] Certificates found. Starting in HTTPS mode...`);
    const options = {
        key: fs.readFileSync(keyPath),
        cert: fs.readFileSync(certPath)
    };
    server = https.createServer(options);
    protocol = 'https';
} else {
    if (process.env.DISABLE_SSL === '1') {
        console.log(`[Export Node ${id}] SSL disabled via environment variable. Starting in HTTP mode...`);
    } else {
        console.log(`[Export Node ${id}] No certificates found. Starting in HTTP mode...`);
    }
    server = http.createServer();
}

console.log(`[Export Node ${id}] Starting Native Mesh Node...`);

const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
await vfs.init();

console.log(`[Export Node ${id}] Starting Native Mesh Node with neighbors: [${neighbors.join(', ')}]`);
const meshLink = new MeshLink(vfs, neighbors, { localUrl: `${protocol}://localhost:${port}` });

/*
// Register the Export Op as a VFS Provider
vfs.registerProvider('jot/pdf', async (v, selector) => {
    try {
        const { $in, path: pdfPath = 'export.pdf' } = selector.parameters;
        const output = selector.output || '$out';

        if (!$in) throw new Error('Missing input $in');

        // 1. Fetch input data
        const data = await v.readData($in);
        if (!data) throw new Error(`Could not find input: ${JSON.stringify($in)}`);

        // 2. Perform the export (Simulated)
        // In a real implementation, this would involve actual PDF generation logic.
        const fileContent = data instanceof Uint8Array ? data : 
                           (typeof data === 'string' ? data : JSON.stringify(data));
        
        console.log(`[Export Node] Export generated for ${pdfPath}`);

        // 3. Fulfill based on requested port
        if (output === 'file') {
            return fileContent;
        }

        // Default: Pass through the input shape (The Tee Pattern)
        return data;

    } catch (err) {
        console.error(`[Export Node ERROR] ${err.message}`);
        return null;
    }
}, {
    schema: {
        path: 'jot/pdf',
        description: 'Exports a shape to a PDF file on the server and provides it for download.',
        arguments: [
            { name: '$in', type: 'jot:shape', affiliate: '$in' },
            { name: 'path', type: 'jot:string', default: 'export.pdf' }
        ],
        outputs: { 
            '$out': { type: 'jot:shape' },
            'file': { type: 'file', mimeType: 'application/pdf' }
        }
    }
});
*/

registerVFSRoutes(vfs, server, '', meshLink);

server.listen(port, '0.0.0.0', async () => {
    console.log(`[Export Node ${id}] Listening on ${protocol}://0.0.0.0:${port}`);
    await meshLink.start();
});
