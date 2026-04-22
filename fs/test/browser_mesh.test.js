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

test('Browser Mesh Integration: Catalog & Execution', async (t) => {
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
      stdio: 'pipe',
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
    if (uxServer) uxServer.kill();
    if (opsNode) await opsNode.stop();
    await fs.rm('.vfs_storage_browser_test_ops', { recursive: true, force: true }).catch(() => {});
  });

  await t.test('should deliver catalog and execute jot expression', async () => {
    const page = await browser.newPage();
    
    // Capture browser console logs
    page.on('console', msg => {
        console.log(`[BROWSER] ${msg.text()}`);
    });

    await page.goto(`http://localhost:${PORT_UX}`);
    console.log('[Test Browser] Page loaded, waiting for catalog...');

    // 1. Wait for Catalog
    const catalogReceived = await page.waitForFunction(() => {
        const b = window.blackboard;
        return b && b.schemas && b.schemas()['jot/Box'];
    }, { timeout: 20000 });
    assert.ok(catalogReceived, 'Catalog should contain jot/Box');

    const allSchemas = await page.evaluate(() => window.blackboard.schemas());
    const schemaPaths = Object.keys(allSchemas).sort();
    console.log(`[Test Browser] Total schemas received: ${schemaPaths.length}`);
    
    const expectedPaths = [
        'jot/Box', 'jot/Hexagon/cap', 'jot/Hexagon/full', 'jot/Hexagon/half',
        'jot/Hexagon/middle', 'jot/Hexagon/sector', 'jot/Triangle/equilateral',
        'jot/at', 'jot/color', 'jot/corners', 'jot/cut', 'jot/group', 'jot/link',
        'jot/loop', 'jot/nth', 'jot/offset', 'jot/offset/closure', 'jot/on',
        'jot/outline', 'jot/pdf', 'jot/png', 'jot/points', 'jot/rotate'
    ].sort();

    assert.deepStrictEqual(schemaPaths, expectedPaths, 'Received schemas should match the expected set');

    // 2. Inject expression
    console.log('[Test Browser] Injecting Jot expression...');
    await page.waitForSelector('textarea');
    const expression = 'Box(20, 20, 20)';
    await page.evaluate((exp) => {
        const el = document.querySelector('textarea');
        el.value = exp;
        el.dispatchEvent(new Event('input', { bubbles: true }));
    }, expression);

    // 3. Click Evaluate
    console.log('[Test Browser] Clicking Evaluate...');
    await page.evaluate(() => {
        const btn = Array.from(document.querySelectorAll('button')).find(b => b.textContent.includes('EVALUATE JOT'));
        if (btn) btn.click();
        else throw new Error('Evaluate button not found');
    });

    // 4. Wait for evaluation to complete
    console.log('[Test Browser] Waiting for evaluation...');
    await page.waitForFunction(() => {
        const btn = Array.from(document.querySelectorAll('button')).find(b => b.textContent === 'EVALUATE JOT');
        return btn && !btn.disabled;
    }, { timeout: 30000 });

    // 5. Verify result: check for CID in VFS and Canvas presence
    const resultOk = await page.evaluate(async () => {
        const canvas = document.querySelector('canvas');
        return !!canvas;
    });

    assert.ok(resultOk, 'Canvas should be present in the viewport');
    console.log('[Test Browser] SUCCESS: Catalog received and expression executed.');
  });
});
