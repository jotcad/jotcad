import { VFS, RESTBridge } from './fs/src/index.js';
import { toPdf } from './geometry/pdf.js';
import { withAssets } from './geometry/assets.js';
import { makeShape, edges } from './geometry/main.js';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const vfs = new VFS({ id: 'pdf-generator' });
const bridge = new RESTBridge(vfs, 'http://localhost:9090/vfs');

async function main() {
    console.log('[PDF] Starting VFS Bridge...');
    await bridge.start();

    const hexPath = 'shape/hexagon';
    
    // 248mm across the flats
    const flats = 248;
    const radius = flats / Math.sqrt(3); 
    const parameters = { radius, kerf: 0.09, variant: 'cap' }; 

    console.log(`[PDF] Demanding hexagon cap (triangle) with params:`, parameters);
    
    // 1. Demand the cap section
    await vfs.tickle(hexPath, parameters);

    // 2. Wait for it to be AVAILABLE and read it
    console.log(`[PDF] Waiting for data...`);
    const shapeData = await vfs.readData(hexPath, parameters);
    
    if (!shapeData) {
        console.error('[PDF] Failed to retrieve shape');
        process.exit(1);
    }

    console.log(`[PDF] Shape retrieved, generating PDF...`);

    // 3. Convert to PDF
    const assetsPath = path.join(os.tmpdir(), 'jotcad_pdf_assets_' + Math.random().toString(36).slice(2));
    await withAssets(assetsPath, async (assets) => {
        const geoVfsPath = shapeData.geometry.replace('vfs:/', '');
        const geoParams = shapeData.parameters || {};
        const geoText = await vfs.readData(geoVfsPath, geoParams);
        
        const geoId = assets.textId(geoText);
        
        // Flatten tags
        const flattenedTags = [];
        if (shapeData.tags) {
            for (const values of Object.values(shapeData.tags)) {
                if (Array.isArray(values)) flattenedTags.push(...values);
                else flattenedTags.push(values);
            }
        }
        if (!flattenedTags.some(t => t.startsWith('color:'))) flattenedTags.push('color:black');
        
        // Reconstruct as a proper Shape object
        let shape = makeShape({ geometry: geoId, tags: flattenedTags });

        // Transform to an outline (segments)
        console.log(`[PDF] Extracting edges...`);
        shape = edges(assets, shape);

        const pdfBuffer = toPdf(assets, shape);
        
        const outPath = 'cap_hexagon_with_kerf.pdf';
        fs.writeFileSync(outPath, pdfBuffer);
        console.log(`[PDF] Successfully wrote ${outPath}`);
    });

    await bridge.stop();
    vfs.close();
}

main().catch(err => {
    console.error('[PDF] Fatal Error:', err);
    process.exit(1);
});
