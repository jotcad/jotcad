import test from 'node:test';
import puppeteer from 'puppeteer';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { launchSystem, PROFILES } from '../../orchestrator.js';
import { log } from '../../fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('Puppeteer Sequence Section Test: Z-axis slice', { timeout: 120000 }, async (t) => {
  let cluster, browser;
  try {
    cluster = await launchSystem('test/standard');
    const PORT_UX = cluster.ports.ux;

    browser = await puppeteer.launch({ 
      headless: 'new',
      args: ['--no-sandbox', '--ignore-certificate-errors'] 
    });
    const page = await browser.newPage();
    await page.setViewport({ width: 1024, height: 768 });

    log(`[Test Browser] Loading UX on port ${PORT_UX}...`);
    
    // Wait for the catalog handshake to succeed in the browser
    log('[Test Browser] Waiting for Catalog handshake...');
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('Handshake timeout')), 45000);
      page.on('console', (msg) => {
        if (msg.text().includes('Received Catalog from geo-ops-node')) {
          clearTimeout(timeout);
          resolve();
        }
      });
      // Force HTTPS as per orchestrator configuration
      page.goto(`https://localhost:${PORT_UX}/`, { waitUntil: 'domcontentloaded' }).catch(reject);
    });

    log('[Test Browser] Catalog handshake SUCCESS.');

    // Wait to ensure Blackboard state transitions are fully settled
    await new Promise(r => setTimeout(r, 2000));

    // Evaluate the sequence rotation and sectioning code directly inside the browser window
    const code = "Box(1, 10, 2).rz([by 1/8]).section(Z(0)) -> $out";
    log(`[Test Browser] Injecting and evaluating code: ${code}`);

    const result = await page.evaluate(async (expr) => {
      const bb = window.blackboard;
      if (!bb) throw new Error("window.blackboard is not defined in the page");

      // 1. Publish the code as a dynamic user operator
      const schema = {
        arguments: [],
        outputs: { "$out": { type: "jot:shape" } }
      };
      await bb.publishDynamicOp("user/seq_section_test:v1", schema, expr);

      // 2. Read the operator from VFS (which triggers provider compilation and execution)
      const selectorObj = { path: "user/seq_section_test:v1", parameters: {}, output: "$out" };
      const selector = window.Selector.fromObject(selectorObj);
      const shapeStream = await bb.vfs.read(selector);

      // 3. Consume resulting shape metadata from VFS
      const reader = shapeStream.stream.getReader();
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
      const shape = JSON.parse(new TextDecoder().decode(bytes));

      return {
         selector: selectorObj,
         tags: shape.tags,
         geometry: shape.geometry || (shape.components && shape.components[0].geometry)
      };
    }, code);

    log("[Test Browser] Evaluation complete:", result);

    if (!result.geometry) {
      throw new Error("Expected evaluated section to contain geometry CID");
    }
    if (!result.tags || result.tags.type !== 'group') {
      throw new Error(`Expected tags to have type 'group', got: ${JSON.stringify(result.tags)}`);
    }

    // Capture browser screenshot to actual/ directory
    log('[Test Browser] Capturing page screenshot...');
    const targetDir = path.resolve(__dirname, '../actual');
    await fs.mkdir(targetDir, { recursive: true });
    const screenshotPath = path.join(targetDir, 'puppeteer_sequence_section_screenshot.png');
    await page.screenshot({ path: screenshotPath });
    log(`[Test Browser] Captured and wrote screenshot to ${screenshotPath}`);

  } finally {
    if (browser) await browser.close();
    if (cluster) await cluster.stop();
  }
});
