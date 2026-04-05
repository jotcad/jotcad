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
    console.log('Seeding VFS with an example triangle...');
    const params = { side: 50, form: 'equilateral' };
    
    // Real OBJ data for a triangle
    const objData = [
        'v 0 0 0',
        'v 50 0 0',
        'v 25 43.3 0',
        'f 1 2 3'
    ].join('\n');
    
    // Write to geo/mesh (where thumbnails are enabled)
    await vfs.write('geo/mesh', { a: 50, b: 50, c: 50, type: 'triangle' }, Readable.from([objData]));
    
    // Write the Shape JSON that points to it
    const shape = {
        geometry: 'vfs:/geo/mesh',
        parameters: { a: 50, b: 50, c: 50, type: 'triangle' },
        tags: ['seeded']
    };
    await vfs.write('shape/triangle', params, Readable.from([JSON.stringify(shape)]));
    
    console.log('Done. Created geo/mesh and shape/triangle.');
    await vfs.close();
}

seed().catch(console.error);
