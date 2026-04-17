import { VFS, MeshLink } from './fs/src/index.js';
import fs from 'node:fs';
import path from 'node:path';

async function main() {
    const vfs = new VFS({ id: 'pdf-test-client' });
    const mesh = new MeshLink(vfs, ['http://localhost:9091']);
    
    await vfs.init();
    await mesh.start();

    console.log('[Test] Requesting Green Hexagon PDF...');

    // Pipeline: jot/Hexagon/full -> jot/color(color="green") -> jot/pdf(filename="green_hex.pdf")
    const pdfSelector = {
        path: 'jot/pdf',
        parameters: {
            filename: 'green_hex.pdf',
            $in: {
                path: 'jot/color',
                parameters: {
                    color: 'green',
                    $in: {
                        path: 'jot/Hexagon/full',
                        parameters: {
                            diameter: 100
                        }
                    }
                }
            }
        }
    };

    console.log('[Test] Reading PDF Selector:', JSON.stringify(pdfSelector, null, 2));
    
    // This triggers the chain
    const shapeResult = await vfs.readData(pdfSelector.path, pdfSelector.parameters);
    
    if (!shapeResult) {
        console.error('[Test] FAILED: Could not resolve PDF op chain');
        process.exit(1);
    }

    console.log('[Test] PDF Op returned Shape (Tee):', JSON.stringify(shapeResult, null, 2));

    console.log('[Test] Retrieving PDF bytes from VFS...');
    const pdfBytes = await vfs.readData('green_hex.pdf', {});

    if (!pdfBytes) {
        console.error('[Test] FAILED: PDF file not found in VFS after op execution');
        process.exit(1);
    }

    const outPath = path.resolve('green_hex.pdf');
    fs.writeFileSync(outPath, pdfBytes);
    
    console.log(`--------------------------------------------------`);
    console.log(`SUCCESS: Green Hexagon PDF written to: ${outPath}`);
    console.log(`Size: ${pdfBytes.length} bytes`);
    console.log(`--------------------------------------------------`);

    mesh.stop();
    vfs.close();
}

main().catch(err => {
    console.error('[Test] Fatal Error:', err);
    process.exit(1);
});
