import test from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import fs from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import puppeteer from 'puppeteer';
import { launchOpsNode } from './ops_helper.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OPS_PATH = path.resolve(__dirname, '../../geo/bin/ops');
const UX_ROOT = path.resolve(__dirname, '../../ux');
const VITE_BIN = path.resolve(__dirname, '../../node_modules/.bin/vite');
const PORT_OPS = 9092;
const PORT_UX = 3030;

test('Browser Mesh Integration: UX receives C++ Catalog', async (t) => {
  let opsNode, uxServer, browser;

  t.before(async () => {
    await fs.rm('.vfs_storage_browser_test_ops', { recursive: true, force: true }).catch(() => {});

    // 1. Launch C++ Ops Node
    console.log('[Test Browser] Launching C++ Native Node...');
    opsNode = await launchOpsNode(OPS_PATH, PORT_OPS, '.vfs_storage_browser_test_ops');

    // 2. Launch UX Vite Server
    console.log('[Test Browser] Launching UX Vite Server...');
    uxServer = spawn(VITE_BIN, ['--port', PORT_UX, '--strictPort'], {
      cwd: UX_ROOT,
      stdio: 'pipe', // Change to pipe to capture output if needed, but 'inherit' for now for visibility
      env: { ...process.env, VITE_VFS_URL: `http://localhost:${PORT_OPS}` }
    });
    
    // Forward UX logs with prefix
    uxServer.stdout.on('data', d => process.stdout.write(`[UX] ${d}`));
    uxServer.stderr.on('data', d => process.stderr.write(`[UX ERR] ${d}`));

    // 3. Wait for UX to be up
    let uxUp = false;
    for (let i = 0; i < 50; i++) {
      try {
        const resp = await fetch(`http://localhost:${PORT_UX}`);
        if (resp.ok) { uxUp = true; break; }
      } catch (e) {}
      await new Promise(r => setTimeout(r, 200));
    }
    if (!uxUp) throw new Error('UX Server failed to start');

    // 4. Launch Puppeteer
    console.log('[Test Browser] Launching Puppeteer...');
    browser = await puppeteer.launch({ 
      headless: true,
      args: ['--no-sandbox', '--disable-setuid-sandbox']
    });
  });

  t.after(async () => {
    console.log('[Test Browser] Cleaning up...');
    if (browser) await browser.close();
    if (uxServer) {
        uxServer.kill();
    }
    if (opsNode) await opsNode.stop();
    await fs.rm('.vfs_storage_browser_test_ops', { recursive: true, force: true }).catch(() => {});
  });

  await t.test('should receive jot/Box schema in browser via Mesh-VFS', async () => {
    const page = await browser.newPage();
    
    // Capture browser console logs
    page.on('console', msg => {
        console.log(`[BROWSER ${msg.type()}] ${msg.text()}`);
    });

    await page.goto(`http://localhost:${PORT_UX}`);
    console.log('[Test Browser] Page loaded, waiting for catalog...');

    // Wait for the schema in window.blackboard.schemas()
    // The C++ node publishes its catalog on startup/registration.
    // The browser subscribes to sys/schema and receives it.
    const success = await page.waitForFunction(() => {
        const b = window.blackboard;
        if (!b) return false;
        const s = b.schemas();
        return s && s['jot/Box'];
    }, { timeout: 20000 });

    assert.ok(success, 'Catalog should contain jot/Box');
    
    // Verify some details of the received schema
    const allSchemas = await page.evaluate(() => window.blackboard.schemas());
    const schemaPaths = Object.keys(allSchemas).sort();
    console.log(`[Test Browser] Total schemas received: ${schemaPaths.length}`);
    
    const expectedPaths = [
        'jot/Box',
        'jot/Hexagon/cap',
        'jot/Hexagon/full',
        'jot/Hexagon/half',
        'jot/Hexagon/middle',
        'jot/Hexagon/sector',
        'jot/Triangle/equilateral',
        'jot/at',
        'jot/color',
        'jot/corners',
        'jot/cut',
        'jot/group',
        'jot/link',
        'jot/loop',
        'jot/nth',
        'jot/offset',
        'jot/offset/closure',
        'jot/on',
        'jot/outline',
        'jot/pdf',
        'jot/png',
        'jot/points',
        'jot/rotate'
    ].sort();

    assert.deepStrictEqual(schemaPaths, expectedPaths, 'Received schemas should match the expected set from C++ node');
    
    // Perform structural sanity check on every schema
    for (const path of expectedPaths) {
        const s = allSchemas[path];
        assert.ok(s, `Schema for ${path} should exist`);
        assert.strictEqual(s.path, path, `Path mismatch for ${path}`);
        assert.ok(s.arguments && typeof s.arguments === 'object', `Schema ${path} should have an arguments object`);
        assert.ok(s.outputs && typeof s.outputs === 'object', `Schema ${path} should have an outputs object`);
        assert.strictEqual(s._origin, 'geo-ops-node', `Origin mismatch for ${path}`);
    }

    const boxSchema = allSchemas['jot/Box'];
    console.log('[Test Browser] Received jot/Box schema:', JSON.stringify(boxSchema, null, 2));
    assert.strictEqual(boxSchema.path, 'jot/Box');
    assert.ok(boxSchema.arguments.width, 'Should have width argument');
    
    console.log('[Test Browser] SUCCESS: Catalog received and verified in browser.');
  });
});
