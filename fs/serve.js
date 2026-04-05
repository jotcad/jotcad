import http from 'node:http';
import { VFS, registerVFSRoutes } from './src/index.js';
import path from 'node:path';
import fs from 'node:fs';

const port = process.env.PORT || 9090;
const storageDir = path.join(process.cwd(), '.vfs_storage');

console.log(`[VFS Hub] Starting...`);
console.log(`[VFS Hub] Storage: ${storageDir}`);

if (!fs.existsSync(storageDir)) {
    fs.mkdirSync(storageDir, { recursive: true });
}

const vfs = new VFS({ 
    id: 'hub',
    storage: new (await import('./src/index.js')).DiskStorage(storageDir)
});

await vfs.init();

const server = http.createServer((req, res) => {
    // CORS Headers for the UX app
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, x-vfs-info, x-vfs-peer-id');
    
    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        return res.end();
    }
});

registerVFSRoutes(vfs, server, '/vfs');

server.on('error', (err) => {
    console.error(`[VFS Hub] Server error: ${err.message}`);
    process.exit(1);
});

server.listen(port, '0.0.0.0', () => {
    console.log(`[VFS Hub] Listening on http://0.0.0.0:${port}/vfs`);
});
