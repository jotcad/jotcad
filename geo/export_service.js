import http from 'node:http';
import https from 'node:https';
import fs from 'node:fs';
import fsPromises from 'node:fs/promises';
import path from 'node:path';
import { VFS, DiskStorage, MeshLink, registerVFSRoutes } from '../fs/src/index.js';

const id = process.env.PEER_ID || 'export-node';
const port = parseInt(process.env.PORT || '9092');
const neighbors = (process.env.NEIGHBORS || '').split(',').filter(Boolean);
const storageDir = path.resolve(`.vfs_storage_${id}`);

const __dirname = path.resolve();
const sslDir = path.join(__dirname, '.ssl');
const keyPath = path.join(sslDir, 'localhost-key.pem');
const certPath = path.join(sslDir, 'localhost-cert.pem');

let server;
let protocol = 'http';

if (fs.existsSync(keyPath) && fs.existsSync(certPath)) {
    console.log(`[Export Node ${id}] Certificates found. Starting in HTTPS mode...`);
    const options = {
        key: fs.readFileSync(keyPath),
        cert: fs.readFileSync(certPath)
    };
    server = https.createServer(options);
    protocol = 'https';
} else {
    console.log(`[Export Node ${id}] No certificates found. Starting in HTTP mode...`);
    server = http.createServer();
}

console.log(`[Export Node ${id}] Starting Native Mesh Node...`);

const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
await vfs.init();

console.log(`[Export Node ${id}] Starting Native Mesh Node with neighbors: [${neighbors.join(', ')}]`);
const meshLink = new MeshLink(vfs, neighbors, { localUrl: `${protocol}://localhost:${port}` });

// Register the Export Op as a VFS Provider
vfs.registerProvider('jot/pdf', async (v, selector) => {
    console.log(`[Export Node] Provisioning PDF Export: ${selector.path}`);
    try {
        const { $in, path: pdfPath = 'export.pdf' } = selector.parameters;
        if (!$in) throw new Error('No input provided for PDF export');

        // 1. Resolve the source bytes from the mesh (matches C++ decode behavior)
        const data = await v.readData($in);
        if (!data) throw new Error(`Could not find input: ${JSON.stringify($in)}`);

        // 2. Perform the export (Write to local disk)
        const outPath = path.resolve(pdfPath);
        const fileContent = typeof data === 'string' ? data : (data instanceof Uint8Array ? data : JSON.stringify(data));
        await fsPromises.writeFile(outPath, fileContent);
        console.log(`[Export Node] Exported to ${outPath}`);

        // 3. Fulfill the request (Tee Pattern: return the input metadata)
        const status = { 
            type: 'sys/export_status',
            exportedAt: new Date().toISOString(), 
            filename: outPath, 
            size: data.length 
        };
        
        return status;

    } catch (err) {
        console.error(`[Export Node ERROR] ${err.message}`);
        return null;
    }
}, {
    schema: {
        path: 'jot/pdf',
        description: 'Exports a shape to a PDF file on the server.',
        arguments: [
            { name: '$in', type: 'jot:shape', affiliate: '$in' },
            { name: 'path', type: 'jot:string', default: 'export.pdf' }
        ],
        outputs: { '$out': { type: 'sys/export_status' } }
    }
});

registerVFSRoutes(vfs, server, '', meshLink);

server.listen(port, '0.0.0.0', async () => {
    console.log(`[Export Node ${id}] Listening on ${protocol}://0.0.0.0:${port}`);
    await meshLink.start();
    
    // Periodically log peer status
    setInterval(() => {
        const peerIds = [...meshLink.peers.keys()];
        console.log(`[Export Node ${id}] Mesh Status: ${peerIds.length} peers connected: [${peerIds.join(', ')}]`);
    }, 10000);
});
