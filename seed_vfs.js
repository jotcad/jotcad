import { VFS, DiskStorage } from './fs/src/index.js';
import path from 'node:path';
import fs from 'node:fs';
import { Readable } from 'stream';

const storageDir = path.join(process.cwd(), '.vfs_storage');
if (!fs.existsSync(storageDir)) fs.mkdirSync(storageDir);

const vfs = new VFS({ 
    id: 'seeder',
    storage: new DiskStorage(storageDir)
});

async function seed() {
    console.log('Seeding VFS with an example box...');
    const params = { width: 50, height: 50, depth: 50 };
    const content = 'Mesh(box, size=50)';
    
    await vfs.write('shape/box', params, Readable.from([content]));
    console.log('Done. Created shape/box with params:', params);
    await vfs.close();
}

seed().catch(console.error);
