import { spawn } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage } from './fs/src/index.js';

async function main() {
    const portCpp = 21001;
    const portJs = 21002;
    const storageCpp = path.resolve('.vfs_storage_trace_cpp');
    const storageJs = path.resolve('.vfs_storage_trace_js');

    await Promise.all([
        fs.rm(storageCpp, { recursive: true, force: true }).catch(() => {}),
        fs.rm(storageJs, { recursive: true, force: true }).catch(() => {})
    ]);

    const opsBin = path.resolve('geo/bin/ops');
    const cppProc = spawn(opsBin, [portCpp.toString(), storageCpp]);
    cppProc.stdout.on('data', d => console.log(`[C++] ${d}`));
    cppProc.stderr.on('data', d => console.error(`[C++ ERR] ${d}`));

    const vfs = new VFS({ id: 'trace-client', storage: new DiskStorage(storageJs) });
    const mesh = new MeshLink(vfs, [`http://localhost:${portCpp}`], { localUrl: `http://localhost:${portJs}` });
    const server = http.createServer();
    registerVFSRoutes(vfs, server, '', mesh);
    server.listen(portJs);
    await vfs.init();
    await mesh.start();

    // Wait for nodes
    await new Promise(r => setTimeout(r, 2000));

    console.log('--- Requesting Box from JS Client ---');
    try {
        const result = await vfs.readData('jot/Box', { width: 10, height: 10, depth: 0 });
        console.log('RESULT:', JSON.stringify(result, null, 2));
    } catch (e) {
        console.error('ERROR:', e);
    }

    cppProc.kill();
    server.close();
    mesh.stop();
    await vfs.close();
}
main();
