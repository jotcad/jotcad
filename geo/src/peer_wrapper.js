import http from 'node:http';
import { spawn } from 'node:child_process';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage } from '../../fs/src/index.js';
import path from 'node:path';
import fs from 'node:fs';

const id = process.env.PEER_ID || 'peer-' + Math.random().toString(36).slice(2);
const port = parseInt(process.env.PORT || '9090');
const neighbors = (process.env.NEIGHBORS || '').split(',').filter(Boolean);
const storageDir = path.join(process.cwd(), `.vfs_storage_${id}`);

console.log(`[MeshWrapper ${id}] Starting Peer...`);
console.log(`[MeshWrapper ${id}] Port: ${port}`);
console.log(`[MeshWrapper ${id}] Neighbors: ${neighbors.join(', ')}`);
console.log(`[MeshWrapper ${id}] Storage: ${storageDir}`);

if (!fs.existsSync(storageDir)) {
    fs.mkdirSync(storageDir, { recursive: true });
}

// 1. Initialize VFS with Disk Storage
const vfs = new VFS({ 
    id,
    storage: new DiskStorage(storageDir)
});

// 2. Initialize MeshLink (Recursive Router)
const localUrl = `http://localhost:${port}/vfs`;
const meshLink = new MeshLink(vfs, neighbors, { localUrl });

// 3. Start Peer Server
const server = http.createServer();
registerVFSRoutes(vfs, server, '/vfs', meshLink);

server.listen(port, '0.0.0.0', async () => {
    console.log(`[MeshWrapper ${id}] Peer Server listening on http://0.0.0.0:${port}/vfs`);
    
    await vfs.init();
    await meshLink.start();

    // 4. Spawn the target process
    const args = process.argv.slice(2);
    if (args.length === 0) {
        console.log(`[MeshWrapper ${id}] No child process specified. Running in server-only mode.`);
        return;
    }

    console.log(`[MeshWrapper ${id}] Spawning child: ${args.join(' ')}`);
    
    // Pass the local Peer Server as the "Hub" for legacy compatibility
    const env = { 
        ...process.env, 
        VFS_HUB_URL: `http://127.0.0.1:${port}/vfs`,
        PEER_ID: id 
    };

    const child = spawn(args[0], args.slice(1), {
        stdio: 'inherit',
        env
    });

    child.on('close', (code) => {
        console.log(`[MeshWrapper ${id}] Child process exited with code ${code}`);
        process.exit(code);
    });
});
