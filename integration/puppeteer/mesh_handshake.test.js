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
    cluster = await launchSystem(PROFILES.TEST);
    const PORT_UX = cluster.ports.ux;

    browser = await puppeteer.launch({ 
      headless: 'new',
      args: ['--no-sandbox', '--ignore-certificate-errors']
    });
    const page = await browser.newPage();

    log(`[Test Browser] Loading UX on port ${PORT_UX}...`);

    // Wait for the catalog receipt log
    log('[Test Browser] Waiting for Catalog handshake...');
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('Handshake timeout')), 45000);
      page.on('console', (msg) => {
        if (msg.text().includes('Received Catalog from geo-ops-node')) {
          clearTimeout(timeout);
          resolve();
        }
      });
      const protocol = cluster.isHttps ? 'https' : 'http';
      page.goto(`${protocol}://localhost:${PORT_UX}/`, { waitUntil: 'domcontentloaded' });
    });

    log('[Test Browser] Catalog handshake SUCCESS.');
  } finally {    if (browser) await browser.close();
    if (cluster) await cluster.stop();
  }
});
