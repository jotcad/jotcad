import http from 'node:http';
import fsPromises from 'node:fs/promises';
import path from 'node:path';
import { VFS, DiskStorage, MeshLink, registerVFSRoutes } from '../fs/src/index.js';

const id = process.env.PEER_ID || 'export-node';
const port = parseInt(process.env.PORT || '9092');
const neighbors = (process.env.NEIGHBORS || '').split(',').filter(Boolean);
const storageDir = path.resolve(`.vfs_storage_${id}`);

console.log(`[Export Node ${id}] Starting Native Mesh Node...`);

const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
await vfs.init();

const meshLink = new MeshLink(vfs, neighbors, { localUrl: `http://localhost:${port}` });

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
});

const server = http.createServer();
registerVFSRoutes(vfs, server, '', meshLink);

server.listen(port, '0.0.0.0', async () => {
    console.log(`[Export Node ${id}] Listening on http://localhost:${port}`);
    await meshLink.start();
});
