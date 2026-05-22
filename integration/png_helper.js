import fs from 'node:fs/promises';
import crypto from 'node:crypto';
import assert from 'node:assert';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Selector } from '../fs/src/index.js';

/**
 * Captures a PNG snapshot from a shape selector and saves it to disk.
 * Optionally verifies the content against a known SHA-256 hash.
 * 
 * @param {Object} vfs - The VFS instance to use for reading.
 * @param {Object} shapeSelector - The selector for the geometry to render.
 * @param {string} filename - The output filename (e.g., 'result.png').
 * @param {string} [expectedHash] - Optional SHA-256 hex string to assert against.
 * @returns {Promise<Buffer>} The PNG bytes.
 */
export async function captureAndVerifyPNG(vfs, shapeSelector, filename, expectedHash = null) {
    console.log(`[PNG Helper] Capturing snapshot for ${filename}...`);
    
    // 1. Create the PNG selector
    const pngSelector = new Selector('jot/png', { '$in': shapeSelector }).withOutput('$out');
    
    // 2. Read from VFS
    const result = await vfs.read(pngSelector);
    if (!result) {
        throw new Error(`Failed to read PNG for ${filename} from VFS`);
    }
    
    // 3. Consume the stream
    const chunks = [];
    for await (const chunk of result.stream) {
        chunks.push(chunk);
    }
    const pngBytes = Buffer.concat(chunks);
    
    assert.ok(pngBytes.length > 0, `PNG ${filename} should not be empty`);
    
    // 4. Write to disk (inside actual/ folder relative to project root)
    const __filename = fileURLToPath(import.meta.url);
    const __dirname = path.dirname(__filename);
    const targetDir = path.resolve(__dirname, 'actual');
    await fs.mkdir(targetDir, { recursive: true });
    const outputPath = path.join(targetDir, filename);
    await fs.writeFile(outputPath, pngBytes);
    console.log(`[PNG Helper] Wrote ${filename} to ${outputPath} (${pngBytes.length} bytes)`);
    
    // 5. Verify hash if provided
    const actualHash = crypto.createHash('sha256').update(pngBytes).digest('hex');
    console.log(`[PNG Helper] ${filename} Hash: ${actualHash}`);
    
    if (expectedHash) {
        assert.strictEqual(actualHash, expectedHash, `PNG content hash mismatch for ${filename}!`);
        console.log(`[PNG Helper] ${filename} Hash Verified.`);
    }
    
    return pngBytes;
}
