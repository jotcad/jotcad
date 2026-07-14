import test from 'node:test';
import puppeteer from 'puppeteer';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { launchSystem } from '../../orchestrator.js';
import { log } from '../../fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('Puppeteer Texture Rendering Test', { timeout: 120000 }, async (t) => {
  let cluster, browser;
  try {
    cluster = await launchSystem('test/standard');
    
    browser = await puppeteer.launch({ 
      headless: 'new',
      args: ['--no-sandbox', '--ignore-certificate-errors'] 
    });
    const page = await browser.newPage();
    await page.setViewport({ width: 1024, height: 768 });

    console.log(`[Test Browser] Loading UX...`);
    
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('Handshake timeout')), 45000);
      page.on('console', (msg) => {
        console.log(`[Browser Console: ${msg.type()}] ${msg.text()}`);
        if (msg.text().includes('Received Catalog from')) {
          clearTimeout(timeout);
          resolve();
        }
      });
      page.on('pageerror', (err) => {
        console.error(`[Browser PageError] ${err.stack || err}`);
      });
      page.goto(cluster.gatewayUrl, { waitUntil: 'domcontentloaded' }).catch(reject);
    });

    console.log('[Test Browser] Catalog handshake SUCCESS.');
    await new Promise(r => setTimeout(r, 2000));

    // Script: Box with a wood material
    const code = 'Box(30, 30, 0).material("wood") -> $out';
    console.log(`[Test Browser] Injecting code: ${code}`);
    
    await page.evaluate(async (expr) => {
      const bb = window.blackboard;
      const schema = { arguments: [], outputs: { "$out": { type: "jot:shape" } } };
      const targetPath = await bb.publishDynamicOp("user/texture_test", schema, expr);
      
      // Open the editor window
      if (window.blackboard && window.blackboard.openOp) {
          window.blackboard.openOp(targetPath);
      }
      
      const selector = window.Selector.fromObject({ path: targetPath, parameters: {}, output: "$out" });
      
      // Force mesh resolution at the VFS level to ensure C++ actually computes it
      await bb.vfs.read(selector);

    }, code);

    // Wait for UI to render the window
    await new Promise(r => setTimeout(r, 2000));

    // Force Evaluation directly
    await page.evaluate(() => {
        const btns = Array.from(document.querySelectorAll('button'));
        const evalBtn = btns.find(b => b.textContent && b.textContent.toUpperCase().includes('EVALUATE JOT'));
        if (evalBtn) {
            console.log('Found Evaluate Button, clicking...');
            evalBtn.click();
        } else {
            console.log('Evaluate Button NOT FOUND!');
        }
    });

    // Wait for the viewport to render
    await new Promise(r => setTimeout(r, 8000));

    console.log('[Test Browser] Capturing screenshot...');
    const targetDir = path.resolve(__dirname, '../actual');
    await fs.mkdir(targetDir, { recursive: true });
    const screenshotPath = path.join(targetDir, 'puppeteer_texture_test.png');
    await page.screenshot({ path: screenshotPath });
    console.log(`[Test Browser] Screenshot saved to ${screenshotPath}`);

    const stat = await fs.stat(screenshotPath);
    if (stat.size < 1000) throw new Error("Screenshot looks too small/empty");

  } finally {
    if (browser) await browser.close();
    if (cluster) await cluster.stop();
  }
});