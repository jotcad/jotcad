import test from 'node:test';
import puppeteer from 'puppeteer';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launchSystem, PROFILES } from '../../orchestrator.js';
import { log } from '../../fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('Mesh Handshake: Catalog Discovery', async (t) => {
  let cluster, browser;
  try {
    cluster = await launchSystem('test/standard');
    const PORT_UX = cluster.ports.ux;

    // VERIFY IDENTITY (Catch Divergence)
    const EXPECTED_OPS_ID = 'geo-ops-node';

    browser = await puppeteer.launch({ 
      headless: 'new',
      args: ['--no-sandbox', '--ignore-certificate-errors']
    });
    const page = await browser.newPage();

    log(`[Test Browser] Loading UX with gateway: ${cluster.gatewayUrl}...`);

    // Wait for the catalog receipt log
    log('[Test Browser] Waiting for Catalog handshake...');
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error(`Handshake timeout (Expected: ${EXPECTED_OPS_ID})`)), 45000);
      page.on('console', (msg) => {
        console.log(`[Browser Console: ${msg.type()}] ${msg.text()}`);
        if (msg.text().includes(`Received Catalog from ${EXPECTED_OPS_ID}`)) {
          clearTimeout(timeout);
          resolve();
        }
      });
      page.on('pageerror', (err) => {
        console.error(`[Browser PageError] ${err.stack || err}`);
      });
      // Force HTTPS as per orchestrator configuration
      page.goto(cluster.gatewayUrl, { waitUntil: 'domcontentloaded' });
    });

    log('[Test Browser] Catalog handshake SUCCESS.');
  } finally {
    if (browser) await browser.close();
    if (cluster) await cluster.stop();
  }
});
