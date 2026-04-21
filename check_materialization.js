import { spawn } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs/promises';

async function main() {
    const port = 19999;
    const storage = path.resolve('.vfs_storage_diag');
    await fs.rm(storage, { recursive: true, force: true }).catch(() => {});
    await fs.mkdir(storage, { recursive: true });

    const opsBin = path.resolve('geo/bin/ops');
    const proc = spawn(opsBin, [port.toString(), storage]);
    
    // Wait for health
    await new Promise(r => setTimeout(r, 2000));

    const selector = { path: 'jot/Box', parameters: { depth: 0, height: 10, width: 10 } };
    console.log('Requesting Box...');
    const resp = await fetch(`http://localhost:${port}/read`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(selector)
    });
    
    if (resp.ok) {
        console.log('Read OK.');
        const files = await fs.readdir(storage);
        console.log('Storage Files:', files);
    } else {
        console.error('Read Failed:', resp.status);
        const text = await resp.text();
        console.error('Error Body:', text);
    }

    proc.kill();
}
main();
