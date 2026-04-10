import { VFS, RESTBridge } from '../../fs/src/index.js';
import { toPdf } from '../../geometry/pdf.js';
import { withAssets } from '../../geometry/assets.js';
import { makeShape, edges } from '../../geometry/main.js';
import os from 'node:os';
import path from 'node:path';

const vfs = new VFS({ id: 'pdf-service' });
const bridge = new RESTBridge(vfs, 'http://localhost:9090/vfs');

async function main() {
    console.log('[PDF Service] Initializing...');
    await bridge.start();

    // 1. Declare Schema
    vfs.declare('op/pdf', {
        type: 'object',
        properties: {
            source: { type: 'string' },
            filename: { type: 'string', default: 'output.pdf' }
        }
    });

    // 2. Advertise LISTENING
    await vfs.receive({
        path: 'op/pdf',
        parameters: {},
        state: 'LISTENING',
        source: 'peer:pdf-service'
    });

    console.log('[PDF Service] Listening for op/pdf requests...');

    // 3. Watch for PENDING requests on op/pdf
    const query = vfs.watch('op/pdf', { states: ['PENDING'] });
    for await (const req of query) {
        console.log(`[PDF Service] Generating PDF for ${req.path}...`);
        try {
            const { source, filename = 'output.pdf' } = req.parameters;
            if (!source) throw new Error('No source provided for PDF generation');

            // Parse source selector
            let srcPath, srcParams;
            if (typeof source === 'object') {
                srcPath = source.path;
                srcParams = source.parameters || {};
            } else {
                // Fallback for string URIs
                srcPath = source;
                srcParams = {};
                if (srcPath.startsWith('vfs:/')) srcPath = srcPath.slice(5);
                const q = srcPath.indexOf('?');
                if (q !== -1) {
                    const queryStr = srcPath.slice(q + 1);
                    srcPath = srcPath.slice(0, q);
                    try { srcParams = JSON.parse(decodeURIComponent(queryStr)); } catch(e) {}
                }
            }

            // Read source shape
            console.log(`[PDF Service] Requesting source shape: ${srcPath}`);
            const shapeData = await vfs.readData(srcPath, srcParams);
            console.log(`[PDF Service] Received shape data for ${srcPath}`);
            if (!shapeData) throw new Error(`Could not read source shape: ${source}`);

            // Generate PDF
            const assetsPath = path.join(os.tmpdir(), 'jotcad_pdf_assets_' + Math.random().toString(36).slice(2));
            console.log(`[PDF Service] Generating PDF buffer for ${req.path}...`);
            const pdfBuffer = await withAssets(assetsPath, async (assets) => {
                const geoVfsPath = shapeData.geometry.replace('vfs:/', '');
                const geoParams = shapeData.parameters || {};
                
                console.log(`[PDF Service] Resolving underlying geometry: ${geoVfsPath}`);
                const geoText = await vfs.readData(geoVfsPath, geoParams);
                console.log(`[PDF Service] Received geometry mesh (${geoText.length} bytes)`);
                
                const geoId = assets.textId(geoText);
                const flattenedTags = [];
                if (shapeData.tags) {
                    const processTag = (t) => {
                        if (!t) return;
                        if (typeof t === 'string') {
                            flattenedTags.push(t);
                        } else if (Array.isArray(t)) {
                            t.forEach(processTag);
                        } else if (typeof t === 'object') {
                            Object.entries(t).forEach(([k, v]) => {
                                if (typeof v === 'string' || typeof v === 'number') {
                                    flattenedTags.push(`${k}:${v}`);
                                } else {
                                    flattenedTags.push(`${k}:${JSON.stringify(v)}`);
                                }
                            });
                        }
                    };
                    processTag(shapeData.tags);
                }
                
                if (!flattenedTags.some(t => typeof t === 'string' && t.startsWith('color:'))) {
                    flattenedTags.push('color:black');
                }
                
                let shape = makeShape({ geometry: geoId, tags: flattenedTags });
                shape = edges(assets, shape);
                return toPdf(assets, shape);
            });

            // Write PDF back to VFS at the requested coordinate
            await vfs.write(req.path, req.parameters, pdfBuffer);
            console.log(`[PDF Service] Successfully fulfilled ${req.path}`);

        } catch (err) {
            console.error(`[PDF Service] Error fulfilling ${req.path}:`, err.message);
            // Optionally mark as FAILED if we had a failed state
        }
    }
}

main().catch(err => {
    console.error('[PDF Service] Fatal Error:', err);
    process.exit(1);
});
