import test from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import fs from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import puppeteer from 'puppeteer';
import { launchSystem, PROFILES } from '../orchestrator.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('Browser Mesh Integration: Catalog & Execution', async (t) => {
  let cluster, browser;

  t.before(async () => {
    // 1. Launch Isolated Test Cluster
    cluster = await launchSystem(PROFILES.TEST);

    // 2. Launch Puppeteer
    console.log('[Test Browser] Launching Puppeteer...');
    browser = await puppeteer.launch({ 
      headless: true,
      args: ['--no-sandbox', '--disable-setuid-sandbox']
    });
  });

  t.after(async () => {
    console.log('[Test Browser] Cleaning up...');
    if (browser) await browser.close();
    if (cluster) await cluster.stop();
  });

await t.test('should deliver catalog, define dynamic op, and execute it via mesh', async () => {
  const PORT_UX = cluster.ports.ux;
  const page = await browser.newPage();
  await page.setViewport({ width: 1280, height: 1024 });

  // Capture ALL browser console logs
  page.on('console', msg => {
      console.log(`[BROWSER ${msg.type()}] ${msg.text()}`);
  });

  await page.goto(`http://localhost:${PORT_UX}`);
  console.log('[Test Browser] Clearing state for clean run...');
  await page.evaluate(() => {
      localStorage.clear();
      sessionStorage.clear();
      window.location.reload();
  });
  // Use a shorter/more reliable wait for navigation
  await page.waitForNavigation({ waitUntil: 'load', timeout: 30000 });

  console.log('[Test Browser] Page reloaded, waiting for catalog...');

  // 1. Wait for Catalog
  await page.waitForFunction(() => {
      const b = window.blackboard;
      return b && b.schemas && b.schemas()['jot/Box'];
  }, { timeout: 20000 });
  console.log('[Test Browser] Catalog loaded.');

  // 2. Define user/Square:v1
  console.log('[Test Browser] Defining user/Square:v1...');
  await page.evaluate(() => {
      window.blackboard.createNewOp('user/Square:v1');
  });
  await page.waitForSelector('div[data-id="user/Square:v1"]', { timeout: 10000 });
  await page.evaluate(() => {
      window.blackboard.updateEditorState('user/Square:v1', {
          code: 'Box(10) -> $out',
          args: [],
          outputs: [{ name: '$out', type: 'jot:shape' }]
      });
  });

  // 3. Define user/Square:v2
  console.log('[Test Browser] Defining user/Square:v2...');
  await page.evaluate(() => {
      window.blackboard.createNewOp('user/Square:v2');
  });
  await page.waitForSelector('div[data-id="user/Square:v2"]', { timeout: 10000 });
  await page.evaluate(() => {
      window.blackboard.updateEditorState('user/Square:v2', {
          code: 'Box(20) -> $out',
          args: [],
          outputs: [{ name: '$out', type: 'jot:shape' }]
      });
  });

  // 4. Print User Ops in Catalog
  await page.evaluate(() => {
      const allSchemas = window.blackboard.schemas();
      const userOps = Object.keys(allSchemas).filter(k => k.startsWith('user/'));
      console.log(`[Test Catalog] Current User Ops: ${JSON.stringify(userOps)}`);
  });

  // 5. Publish v2 to Mesh
  console.log('[Test Browser] Publishing v2 to mesh...');
  await page.evaluate(() => {
      const win = document.querySelector('div[data-id="user/Square:v2"]');
      const pubBtn = Array.from(win.querySelectorAll('button')).find(b => b.title && b.title.includes('Publish'));
      if (pubBtn) pubBtn.click();
      else console.error('[Test Browser] Could not find Publish button for v2');
  });

  await page.waitForFunction(() => {
      const win = document.querySelector('div[data-id="user/Square:v2"]');
      const btn = Array.from(win.querySelectorAll('button')).find(b => b.title && b.title.includes('Published to Mesh'));
      return !!btn;
  }, { timeout: 15000 });

  // 6. Evaluate v2
  console.log('[Test Browser] Evaluating user/Square:v2...');
  await page.evaluate(() => {
      const win = document.querySelector('div[data-id="user/Square:v2"]');
      const evalBtn = Array.from(win.querySelectorAll('button')).find(b => b.textContent.includes('Evaluate'));
      if (evalBtn) evalBtn.click();
      else console.error('[Test Browser] Could not find Evaluate button for v2');
  });

  // 7. Wait for result
  await page.waitForFunction(() => {
      const win = document.querySelector('div[data-id="user/Square:v2"]');
      const btn = Array.from(win.querySelectorAll('button')).find(b => b.textContent === 'Evaluate Jot');
      return btn && !btn.disabled;
  }, { timeout: 30000 });
    
    // 7. Click the viewport to activate the shared renderer/canvas
    console.log('[Test Browser] Activating viewport for v2...');
    await page.evaluate(() => {
        const win = document.querySelector('div[data-id="user/Square:v2"]');
        const viewport = win.querySelector('.viewport-container');
        if (viewport) viewport.click();
        else console.error('[Test Browser] Could not find viewport-container in v2 window');
    });

    // Give Three.js a moment to upload geometry and render a frame
    await new Promise(r => setTimeout(r, 2000));

    // 8. Capture full screen for documentation BEFORE assertion
    const screenPath = path.resolve(__dirname, '../../ux/browser_integration_screenshot.png');
    await page.screenshot({ path: screenPath, fullPage: true });
    console.log(`[Test Browser] UI screenshot captured to ${screenPath}`);

    // 9. Capture rendered result (Canvas or Img)
    console.log('[Test Browser] Capturing rendered result...');
    const result = await page.evaluate(async () => {
        const win = document.querySelector('div[data-id="user/Square:v2"]');
        const canvas = win.querySelector('canvas');
        const img = win.querySelector('img');
        
        if (canvas) return { type: 'canvas', data: canvas.toDataURL('image/png') };
        if (img) return { type: 'img', data: img.src };
        
        return { error: 'Neither Canvas nor Img found in viewport' };
    });

    if (result.error) {
        console.error(`[Test Browser] ERROR: ${result.error}`);
        console.log(`[Test Browser] Elements found: ${JSON.stringify(result.elements, null, 2)}`);
    }

    assert.ok(result.data, 'Result data (canvas or img) should be present');
    
    const base64Data = result.data.replace(/^data:image\/png;base64,/, "");
    const pngPath = path.resolve(__dirname, '../../ux/user_square_result.png');
    await fs.writeFile(pngPath, base64Data, 'base64');
    
    console.log(`[Test Browser] SUCCESS: Dynamic op result captured to ${pngPath}`);
  });
});
