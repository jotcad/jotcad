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
const PORT_OPS = parseInt(process.env.TEST_OPS_PORT || '9092');
const PORT_UX = parseInt(process.env.TEST_UX_PORT || '3033');

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
      env: { 
        ...process.env, 
        VITE_VFS_URL: `http://localhost:${PORT_OPS}`,
        VITE_HTTPS: 'false'
      }
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
    if (browser) {
      console.log('[Test Browser] Closing browser...');
      await browser.close();
    }
    if (uxServer) {
      console.log('[Test Browser] Killing UX server...');
      uxServer.kill();
    }
    if (opsNode) {
      console.log('[Test Browser] Stopping Ops node...');
      await opsNode.stop();
    }
    console.log('[Test Browser] Removing temporary storage...');
    await fs.rm('.vfs_storage_browser_test_ops', { recursive: true, force: true }).catch(() => {});
    console.log('[Test Browser] Cleanup complete.');
  });

await t.test('should deliver catalog, define dynamic op, and execute it via mesh', async () => {
  const page = await browser.newPage();
  await page.setViewport({ width: 1280, height: 1024 });

  // Capture ALL browser console logs
  page.on('console', msg => {
      console.log(`[BROWSER ${msg.type()}] ${msg.text()}`);
  });

  await page.goto(`http://localhost:${PORT_UX}`);
  console.log('[Test Browser] Page loaded, waiting for catalog...');

  // 1. Wait for Catalog
  await page.waitForFunction(() => {
      const b = window.blackboard;
      return b && b.schemas && b.schemas()['jot/Box'];
  }, { timeout: 20000 });

  // 2. Define Dynamic Op: user/Square(side)
  console.log('[Test Browser] Defining dynamic op...');
  await page.evaluate(() => {
      window.blackboard.createNewOp('user/Square');
  });

  // Wait for editor to appear
  await page.waitForSelector('[data-testid="op-name-input"]');

  await page.evaluate(() => {
      const argNameInput = document.querySelector('[data-testid="arg-name-0"]');
      argNameInput.value = 'side';
      argNameInput.dispatchEvent(new Event('input', { bubbles: true }));

      const codeEditor = document.querySelector('textarea');
      codeEditor.value = 'Box(side, side, side).rotate(45)';
      codeEditor.dispatchEvent(new Event('input', { bubbles: true }));
  });


    // 3. Publish to Mesh
    console.log('[Test Browser] Publishing to mesh...');
    await page.click('button[title*="Publish"]');

    // Wait for the button title to change to "Published to Mesh" indicating success
    await page.waitForSelector('button[title="Published to Mesh"]', { timeout: 10000 });

    // 4. Verify catalog update
    await page.waitForFunction(() => {
        const b = window.blackboard;
        return b && b.schemas()['user/Square'];
    }, { timeout: 5000 });
    console.log('[Test Browser] Dynamic op published and visible in catalog.');

    // 5. Execute custom op via mesh
    console.log('[Test Browser] Executing custom op...');
    await page.evaluate(() => {
        const codeEditor = document.querySelector('textarea');
        codeEditor.value = 'user/Square(50)';
        codeEditor.dispatchEvent(new Event('input', { bubbles: true }));
        
        const btn = Array.from(document.querySelectorAll('button')).find(b => b.textContent.includes('EVALUATE JOT'));
        if (btn) btn.click();
    });

    // 6. Wait for result and render stability
    await page.waitForFunction(() => {
        const btn = Array.from(document.querySelectorAll('button')).find(b => b.textContent === 'EVALUATE JOT');
        return btn && !btn.disabled;
    }, { timeout: 30000 });
    
    // Give Three.js a moment to upload geometry and render a frame
    await new Promise(r => setTimeout(r, 1000));

    // 7. Capture rendered result from Canvas
    console.log('[Test Browser] Capturing rendered result...');
    const pngDataUrl = await page.evaluate(async () => {
        const canvas = document.querySelector('canvas');
        if (!canvas) return null;
        
        // Force a synchronous render loop completion if possible
        // but since we are external, we'll just capture.
        return canvas.toDataURL('image/png');
    });

    assert.ok(pngDataUrl, 'Canvas should be present with rendered result');
    
    const base64Data = pngDataUrl.replace(/^data:image\/png;base64,/, "");
    const pngPath = path.resolve(__dirname, '../../ux/user_square_result.png');
    await fs.writeFile(pngPath, base64Data, 'base64');
    
    // 8. Capture full screen for documentation
    const screenPath = path.resolve(__dirname, '../../ux/browser_integration_screenshot.png');
    await page.screenshot({ path: screenPath, fullPage: true });
    
    console.log(`[Test Browser] SUCCESS: UI screenshot captured to ${screenPath}`);
    console.log(`[Test Browser] SUCCESS: Dynamic op result captured to ${pngPath}`);
  });
});
