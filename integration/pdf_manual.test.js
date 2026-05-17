import { VFS, MeshLink } from '../fs/src/index.js';
import fs from 'node:fs';
import path from 'node:path';

async function consumeBinary(stream) {
    const reader = stream.getReader();
    const chunks = [];
    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
    }
    const len = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(len);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    return bytes;
}

async function main() {
    const vfs = new VFS({ id: 'pdf-test-client' });
    const mesh = new MeshLink(vfs, ['http://localhost:20301']);
    
    await vfs.init();
    await mesh.start();

    console.log('[Test] Requesting Green Hexagon PDF...');

    // Pipeline: jot/Hexagon/full -> jot/color -> jot/pdf
    const pdfSelector = {
        path: 'jot/pdf',
        parameters: {
            path: 'green_hex.pdf',
            $in: {
                path: 'jot/color',
                parameters: {
                    color: 'green',
                    $in: {
                        path: 'jot/Hexagon/diameter',
                        parameters: {
                            diameter: 100
                        }
                    }
                }
            }
        }
    };

    console.log('[Test] Reading PDF Selector:', JSON.stringify(pdfSelector, null, 2));
    
    // This triggers the chain and returns the PDF bytes directly
    const result = await vfs.read(pdfSelector);
    if (!result) {
        console.error('[Test] FAILED: Could not resolve PDF op chain');
        process.exit(1);
    }
    const pdfBytes = await consumeBinary(result.stream);
    
    if (!pdfBytes) {
        console.error('[Test] FAILED: Could not retrieve bytes');
        process.exit(1);
    }

    const outPath = path.resolve('green_hex.pdf');
    fs.writeFileSync(outPath, pdfBytes);
    
    console.log(`--------------------------------------------------`);
    console.log(`SUCCESS: Green Hexagon PDF written to: ${outPath}`);
    console.log(`Size: ${pdfBytes.length} bytes`);
    console.log(`--------------------------------------------------`);

    mesh.stop();
    await vfs.close();
}

main().catch(err => {
    console.error('[Test] Fatal Error:', err);
    process.exit(1);
});
