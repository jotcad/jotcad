import test from 'node:test';
import puppeteer from 'puppeteer';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { launchSystem } from '../../orchestrator.js';
import { log } from '../../fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('Puppeteer Translucency Test: Ghost Overlap', { timeout: 120000 }, async (t) => {
  let cluster, browser;
  try {
    cluster = await launchSystem('test/standard');
    
    browser = await puppeteer.launch({ 
      headless: 'new',
      args: ['--no-sandbox', '--ignore-certificate-errors'] 
    });
    const page = await browser.newPage();
    await page.setViewport({ width: 800, height: 600 });

    log(`[Test Browser] Loading UX...`);
    
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('Handshake timeout')), 45000);
      page.on('console', (msg) => {
        if (msg.text().includes('Received Catalog from')) {
          clearTimeout(timeout);
          resolve();
        }
      });
      page.goto(cluster.gatewayUrl, { waitUntil: 'domcontentloaded' }).catch(reject);
    });

    log('[Test Browser] Catalog handshake SUCCESS.');
    await new Promise(r => setTimeout(r, 2000));

    // Script: Red Box at Z=0, Blue Ghost Box at Z=5 (closer to camera)
    // Camera is top-down by default.
    const code = 'Box(20, 20, 2).color("red") -> R; Box(15, 15, 2).mz(5).ghost().color("blue") -> B; [R, B] -> $out';
    log(`[Test Browser] Injecting code: ${code}`);

    await page.evaluate(async (expr) => {
      const bb = window.blackboard;
      const schema = { arguments: [], outputs: { "$out": { type: "jot:shape" } } };
      const targetPath = await bb.publishDynamicOp("user/translucency_test", schema, expr);
      const selector = window.Selector.fromObject({ path: targetPath, parameters: {}, output: "$out" });
      
      // Trigger execution and wait for it to be rendered
      await bb.vfs.read(selector);
      
      // Wait for rendering to settle
      await new Promise(r => setTimeout(r, 5000));
    }, code);

    log('[Test Browser] Capturing screenshot...');
    const targetDir = path.resolve(__dirname, '../actual');
    await fs.mkdir(targetDir, { recursive: true });
    const screenshotPath = path.join(targetDir, 'translucency_ghost_test.png');
    await page.screenshot({ path: screenshotPath });
    log(`[Test Browser] Screenshot saved to ${screenshotPath}`);

    // Verification: We'd ideally do pixel diff, but for now we'll check if the file exists.
    const stat = await fs.stat(screenshotPath);
    if (stat.size < 1000) throw new Error("Screenshot looks too small/empty");

  } finally {
    if (browser) await browser.close();
    if (cluster) await cluster.stop();
  }
});
