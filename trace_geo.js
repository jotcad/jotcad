import { spawn } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';
import { VFS, MeshLink, DiskStorage } from './fs/src/index.js';

async function main() {
    const portCpp = 23001;
    const storageCpp = path.resolve('.vfs_storage_trace_geo');
    await fs.rm(storageCpp, { recursive: true, force: true }).catch(() => {});

    const opsBin = path.resolve('geo/bin/ops');
    const cppProc = spawn(opsBin, [portCpp.toString(), storageCpp]);
    
    await new Promise(r => setTimeout(r, 1000));

    const vfs = new VFS({ id: 'trace-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${portCpp}`]);
    await vfs.init();
    await mesh.start();

    console.log('--- Requesting Box ---');
    const result = await vfs.readData('jot/Box', { width: 20, height: 20, depth: 0 });
    const geo = await vfs.readData('geo/mesh', result.geometry.parameters);
    console.log('RAW GEO:\n[' + geo + ']');

    cppProc.kill();
    mesh.stop();
    await vfs.close();
}
main();
