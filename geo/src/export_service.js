import http from 'node:http';
import fsPromises from 'node:fs/promises';
import path from 'node:path';
import { VFS, DiskStorage, MeshLink, registerVFSRoutes } from '../../fs/src/index.js';

const id = process.env.PEER_ID || 'export-node';
const port = parseInt(process.env.PORT || '9092');
const neighbors = (process.env.NEIGHBORS || '').split(',').filter(Boolean);
const storageDir = path.resolve(`.vfs_storage_${id}`);

console.log(`[Export Node ${id}] Starting Native Mesh Node...`);

const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
const meshLink = new MeshLink(vfs, neighbors, { localUrl: `http://localhost:${port}` });

// Register the Export Op as a VFS Provider
vfs.registerProvider('op/export', async (v, selector) => {
    console.log(`[Export Node] Provisioning Export: ${selector.path}`);
    try {
        const { source, filename = 'download.bin' } = selector.parameters;
        if (!source) throw new Error('No source provided for export');

        // 1. Resolve the source bytes from the mesh
        // Note: v.readData will use meshLink to find it if it's not local!
        const data = await v.readData(source);
        if (!data) throw new Error(`Could not find source: ${JSON.stringify(source)}`);

        // 2. Perform the export (Write to local disk)
        const outPath = path.resolve(filename);
        await fsPromises.writeFile(outPath, data);
        console.log(`[Export Node] Exported to ${outPath}`);

        // 3. Return the status object as the "file" content
        const status = { 
            exportedAt: new Date().toISOString(), 
            filename: outPath, 
            size: data.length 
        };
        const statusBytes = new TextEncoder().encode(JSON.stringify(status, null, 2));
        return new ReadableStream({
            start(c) { c.enqueue(statusBytes); c.close(); }
        });

    } catch (err) {
        console.error(`[Export Node ERROR] ${err.message}`);
        return null;
    }
});

const server = http.createServer();
registerVFSRoutes(vfs, server, '', meshLink);

server.listen(port, '0.0.0.0', async () => {
    console.log(`[Export Node ${id}] Listening on http://localhost:${port}`);
    await vfs.init();
    await meshLink.start();
});
