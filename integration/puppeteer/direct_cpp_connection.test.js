import test from 'node:test';
import puppeteer from 'puppeteer';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launchSystem, PROFILES } from '../../orchestrator.js';
import { log } from '../../fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('Direct C++ Connection: UX to Ops Node Discovery', async (t) => {
  let cluster, browser;
  try {
    cluster = await launchSystem('live/direct_cpp');
    const PORT_UX = cluster.ports.ux;

    // VERIFY IDENTITY (Catch Divergence)
    const EXPECTED_OPS_ID = 'geo-ops-node';

    browser = await puppeteer.launch({ 
      headless: 'new',
      args: ['--no-sandbox', '--ignore-certificate-errors']
    });
    const page = await browser.newPage();

    log(`[Test Browser] Loading UX on port ${PORT_UX}...`);

    // Wait for the direct catalog receipt log
    log('[Test Browser] Waiting for Direct Catalog handshake...');
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error(`Direct handshake timeout (Expected: ${EXPECTED_OPS_ID})`)), 45000);
      page.on('console', (msg) => {
        if (msg.text().includes(`Received Catalog from ${EXPECTED_OPS_ID}`)) {
          clearTimeout(timeout);
          resolve();
        }
      });
      page.goto(`https://localhost:${PORT_UX}/`, { waitUntil: 'domcontentloaded' });
    });

    log('[Test Browser] Direct C++ Catalog handshake SUCCESS.');
  } finally {
    if (browser) await browser.close();
    if (cluster) await cluster.stop();
  }
});
