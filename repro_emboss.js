import { VFS, MeshLink, DiskStorage, Selector } from './fs/src/index.js';
import { launchSystem } from './orchestrator.js';
import { decodeGeometry } from './jot/src/index.js';

async function consumeBytes(stream) {
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
    console.log("Launching system...");
    const sys = await launchSystem('test/standard');
    const OPS_URL = `http://localhost:${sys.ports.ops}`;

    const vfs = new VFS({
        id: 'emboss-test-node',
        storage: new DiskStorage('.vfs_storage_test_emboss_client'),
    });
    const mesh = new MeshLink(vfs, [OPS_URL]);
    await mesh.start();
    
    await new Promise(r => setTimeout(r, 2000));

    try {
        // 1. Create a Cylinder Subject (Radius 10, Height 20)
        // Note: We use a box or something simple first.
        const subject_sel = new Selector('jot/Box', { width: 30, height: 30, depth: 10 }).withOutput('$out');
        
        // 2. Create a Disk Pattern (Diameter 10)
        const pattern_sel = new Selector('jot/Disk', { diameter: 10, zag: 0.5 }).withOutput('$out');

        // 3. Emboss!
        const emboss_sel = new Selector('jot/emboss', {
                $in: subject_sel,
                pattern: pattern_sel,
                depth: 2.0,
                offset: -0.1 // Sunk in slightly for robust boolean
        }).withOutput('$out');

        console.log("Requesting embossed geometry...");
        const response = await vfs.read(emboss_sel);
        if (!response) throw new Error("vfs.read returned null");
        
        const resultBytes = await consumeBytes(response.stream);
        const result = JSON.parse(new TextDecoder().decode(resultBytes));
        
        if (result.geometry) {
            const geoRes = await vfs.read(result.geometry);
            const geoBytes = await consumeBytes(geoRes.stream);
            const geo = decodeGeometry(new TextDecoder().decode(geoBytes));
            
            console.log("Result Vertex Count:", geo.vertices.length);
            console.log("Result Triangle Count:", geo.triangles.length);
            
            // Check max Z. Box depth 10 is centered at Z=0? 
            // In JotCAD, Box(30,30,10) is centered at [0,0,0], so Z goes from -5 to 5.
            // The Disk pattern is at Z=0.
            // Emboss projects pattern onto surface. Surface of box is at Z=5.
            // With depth 2, max Z should be 7.
            
            let maxZ = -1e18;
            for (const v of geo.vertices) if (v.z > maxZ) maxZ = v.z;
            console.log("Max Z detected:", maxZ);
            
            if (maxZ > 6.85 && maxZ < 7.1) {
                console.log("SUCCESS: Emboss detected at correct height.");
            } else {
                console.log("FAILURE: Unexpected height:", maxZ);
            }
        }

    } catch (e) {
        console.error("Test failed:", e);
    } finally {
        await vfs.close();
        await sys.stop();
    }
}

main();
