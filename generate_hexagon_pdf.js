import { VFS, MeshLink } from './fs/src/index.js';
import { toPdf } from './geometry/pdf.js';
import { withAssets } from './geometry/assets.js';
import { makeShape, edges } from './geometry/main.js';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const vfs = new VFS({ id: 'pdf-generator' });
const mesh = new MeshLink(vfs, ['http://localhost:9092/vfs']);

async function main() {
    console.log('[PDF] Starting Mesh Bridge...');
    await vfs.init();
    await mesh.start();

    const hexPath = 'shape/hexagon';
    
    // 248mm across the flats means diameter of the inscribed circle is 248.
    // Apothem (a) = 124.
    // Radius (R) = a / cos(30 deg) = 124 / (sqrt(3)/2) = 248 / sqrt(3)
    const flats = 248;
    const radius = flats / Math.sqrt(3); 
    const parameters = { radius, kerf: 0.09 }; 

    console.log(`[PDF] Demanding ${hexPath} with params:`, parameters);
    
    // readData in vfs_core.js handles JSON parsing and recursive mesh search
    const shapeData = await vfs.readData(hexPath, parameters);
    
    if (!shapeData) {
        console.error('[PDF] Failed to retrieve shape');
        process.exit(1);
    }

    console.log(`[PDF] Shape retrieved, generating PDF...`);

    // 3. Convert to PDF
    const assetsPath = path.join(os.tmpdir(), 'jotcad_pdf_assets_' + Math.random().toString(36).slice(2));
    await withAssets(assetsPath, async (assets) => {
        // The shape contains a vfs:/ geo link. Resolve it.
        const geoVfsPath = shapeData.geometry.replace('vfs:/', '');
        const geoParams = shapeData.parameters || {};
        const geoText = await vfs.readData(geoVfsPath, geoParams);
        
        const geoId = assets.textId(geoText);
        
        // Flatten tags from object structure { key: [val1, val2] } to [val1, val2]
        const flattenedTags = [];
        if (shapeData.tags) {
            for (const values of Object.values(shapeData.tags)) {
                if (Array.isArray(values)) {
                    flattenedTags.push(...values);
                } else {
                    flattenedTags.push(values);
                }
            }
        }
        
        // Add a default color if none exists so it's visible in PDF
        if (!flattenedTags.some(t => t.startsWith('color:'))) {
            flattenedTags.push('color:black');
        }
        
        // Reconstruct as a proper Shape object
        let shape = makeShape({ geometry: geoId, tags: flattenedTags });

        // Transform to an outline (segments)
        console.log(`[PDF] Extracting edges...`);
        shape = edges(assets, shape);

        const pdfBuffer = toPdf(assets, shape);
        
        const outPath = 'hexagon_with_kerf.pdf';
        fs.writeFileSync(outPath, pdfBuffer);
        console.log(`[PDF] Successfully wrote ${outPath} (Radius: ${parameters.radius}mm, Kerf: ${parameters.kerf}mm)`);
    });

    mesh.stop();
    vfs.close();
}

main().catch(err => {
    console.error('[PDF] Fatal Error:', err);
    process.exit(1);
});
