import test from 'node:test';
import puppeteer from 'puppeteer';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { launchSystem } from '../../orchestrator.js';
import { log } from '../../fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('Puppeteer STL Download Test', { timeout: 120000 }, async (t) => {
  let cluster, browser, page;
  const downloadPath = path.resolve(__dirname, '../actual/downloads');
  
  // Clean up and recreate download path
  try {
      await fs.rm(downloadPath, { recursive: true, force: true });
  } catch (e) {}
  await fs.mkdir(downloadPath, { recursive: true });

  try {
    cluster = await launchSystem('test/standard');
    
    browser = await puppeteer.launch({ 
      headless: 'new',
      args: ['--no-sandbox', '--ignore-certificate-errors'] 
    });
    page = await browser.newPage();
    
    // Set download path via CDP
    const client = await page.target().createCDPSession();
    await client.send('Browser.setDownloadBehavior', {
      behavior: 'allow',
      downloadPath: downloadPath,
    });

    await page.setViewport({ width: 1280, height: 1024 });

    log(`[Test Browser] Loading UX from ${cluster.gatewayUrl}...`);
    
    page.on('console', (msg) => {
        log(`[Browser Console] ${msg.text()}`);
    });

    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('Handshake timeout')), 45000);
      const listener = (msg) => {
        if (msg.text().includes('Received Catalog from')) {
          resolve();
        }
      };
      page.on('console', listener);
      page.goto(cluster.gatewayUrl, { waitUntil: 'domcontentloaded' }).catch(reject);
    });

    log('[Test Browser] Catalog handshake SUCCESS.');
    
    // Brief pause for UI stabilization
    await new Promise(r => setTimeout(r, 2000));

    // Clear any previous state to ensure a clean test
    await page.evaluate(async () => {
        localStorage.clear();
        if (window.indexedDB && window.indexedDB.databases) {
            const dbs = await window.indexedDB.databases();
            for (const db of dbs) {
                window.indexedDB.deleteDatabase(db.name);
            }
        }
    });

    // Script: Simple Box to STL. 
    const code = 'Box(width=20, height=20, depth=20).stl("puppeteer_test.stl") -> $out';
    log(`[Test Browser] Injecting code: ${code}`);

    await page.evaluate(async (expr) => {
      const bb = window.blackboard;
      // We manually publish a dynamic op and open it
      const schema = { 
          arguments: [], 
          outputs: { "$out": { type: "jot:file" } } 
      };
      const targetPath = await bb.publishDynamicOp("user/stl_test", schema, expr);
      
      if (bb.openOp) {
          bb.openOp(targetPath);
      }
    }, code);

    // Wait for the window to appear and stabilize
    await new Promise(r => setTimeout(r, 4000));

    // FORCE set the editor value and wait for sync
    log('[Test Browser] Force-setting editor value...');
    await page.evaluate(async (c) => {
        const bb = window.blackboard;
        const editors = bb.openEditors();
        const topWin = editors.reduce((a, b) => (a.zIndex > b.zIndex ? a : b), editors[0]);
        const activeId = topWin ? topWin.id : null;
        
        if (activeId) {
            bb.updateEditorState(activeId, { code: c });
        }
        const editor = document.querySelector('textarea');
        if (editor) {
            editor.value = c;
            editor.dispatchEvent(new Event('input', { bubbles: true }));
        }
    }, code);

    // Click the "EVALUATE JOT" button
    log('[Test Browser] Triggering Evaluation...');
    await page.evaluate(() => {
        const btns = Array.from(document.querySelectorAll('button'));
        const evalBtn = btns.find(b => b.textContent && b.textContent.toUpperCase().includes('EVALUATE JOT'));
        if (evalBtn) {
            evalBtn.click();
        } else {
            throw new Error('Could not find Evaluate Jot button');
        }
    });

    // Wait for evaluation to finish
    log('[Test Browser] Waiting for evaluation to complete...');
    await page.waitForFunction(() => {
        const btns = Array.from(document.querySelectorAll('button'));
        const evalBtn = btns.find(b => b.textContent && (b.textContent.toUpperCase().includes('EVALUATE JOT') || b.textContent.toUpperCase().includes('EVALUATING')));
        return evalBtn && evalBtn.textContent.toUpperCase().includes('EVALUATE JOT') && !evalBtn.disabled;
    }, { timeout: 30000 });

    // Check if evaluation actually produced an error in the UI
    const evalError = await page.evaluate(() => {
        const results = document.querySelectorAll('.text-red-400');
        return results.length > 0 ? results[0].textContent : null;
    });

    if (evalError) {
        throw new Error(`Evaluation failed in UI: ${evalError}`);
    }

    // Wait for the download button to appear in the ResultList
    const fileName = 'puppeteer_test.stl';
    log(`[Test Browser] Waiting for download button for ${fileName}...`);
    
    try {
        await page.waitForFunction((text) => {
            const btns = Array.from(document.querySelectorAll('button'));
            return btns.some(b => b.textContent && b.textContent.includes(text));
        }, { timeout: 30000 }, fileName);
    } catch (e) {
        log('[Test Browser] Timeout waiting for download button. Taking screenshot...');
        const errorScreenshot = path.resolve(__dirname, '../actual/stl_download_timeout.png');
        await page.screenshot({ path: errorScreenshot });
        throw e;
    }

    // Click the download button
    log('[Test Browser] Clicking download button...');
    await page.evaluate((text) => {
        const btns = Array.from(document.querySelectorAll('button'));
        const dlBtn = btns.find(b => b.textContent && b.textContent.includes(text));
        if (dlBtn) {
            dlBtn.click();
        } else {
            throw new Error('Could not find download button after waiting');
        }
    }, fileName);

    log('[Test Browser] Download triggered. Monitoring filesystem...');

    const expectedFile = path.join(downloadPath, fileName);
    let success = false;
    for (let i = 0; i < 20; i++) {
        try {
            const stat = await fs.stat(expectedFile);
            if (stat.size > 0) {
                log(`[Test Browser] Found file: ${fileName} (${stat.size} bytes)`);
                success = true;
                break;
            }
        } catch (e) {
            // File not yet present
        }
        await new Promise(r => setTimeout(r, 1000));
    }

    if (!success) {
        throw new Error(`Download failed: ${fileName} not found in ${downloadPath}`);
    }

    // VALIDATION: Read the file and check for STL signatures
    log('[Test Browser] Validating STL file content...');
    const content = await fs.readFile(expectedFile, 'utf8');
    if (content.includes('solid')) {
        log('[Test Browser] Verified: ASCII STL signature found.');
    } else {
        // Check for binary header (80 bytes of header + 4 bytes of facet count)
        const buffer = await fs.readFile(expectedFile);
        if (buffer.length >= 84) {
            const facetCount = buffer.readUInt32LE(80);
            log(`[Test Browser] Verified: Binary STL signature found. Facet count: ${facetCount}`);
            if (facetCount === 0) throw new Error('Binary STL has zero facets');
        } else {
            throw new Error('File content does not look like a valid ASCII or Binary STL');
        }
    }

    log('[Test Browser] Puppeteer STL Download Test PASSED.');

  } catch (err) {
    console.error(`[Test Browser] Test Failed: ${err.message}`);
    const errorScreenshot = path.resolve(__dirname, '../actual/stl_download_error.png');
    if (page) await page.screenshot({ path: errorScreenshot });
    throw err;
  } finally {
    if (browser) await browser.close();
    if (cluster) await cluster.stop();
    if (!!t.result?.error) process.exit(1);
  }
});
