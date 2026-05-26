import test from 'node:test';
import puppeteer from 'puppeteer';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launchSystem, PROFILES } from '../../orchestrator.js';
import { log } from '../../fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('Direct C++ Connection: UX to Ops Node Discovery', { timeout: 45000 }, async (t) => {
  let cluster, browser;
  try {
    // Launch cluster using DIRECT_CPP profile (skips Export Node)
    cluster = await launchSystem('live/direct_cpp');
    const PORT_UX = cluster.ports.ux;

    browser = await puppeteer.launch({ 
      headless: 'new',
      args: ['--no-sandbox', '--ignore-certificate-errors']
    });
    const page = await browser.newPage();

    log(`[Test Browser] Loading UX on port ${PORT_UX}...`);

    // Wait for the catalog receipt log from the C++ node
    log('[Test Browser] Waiting for Direct Catalog handshake from geo-ops-node...');
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(new Error('Direct handshake timeout')), 40000);
      page.on('console', (msg) => {
        const text = msg.text();
        log(`[Browser Console] ${msg.type()}: ${text}`);
        if (text.includes('Received Catalog from geo-ops-node')) {
          log('[Test Browser] MATCH: Received Catalog from geo-ops-node');
          clearTimeout(timeout);
          resolve();
        }
      });
      // Force HTTPS as per orchestrator configuration
      page.goto(`https://localhost:${PORT_UX}/`, { waitUntil: 'domcontentloaded' });
    });

    log('[Test Browser] Direct C++ Catalog handshake SUCCESS.');
  } finally {
    if (browser) await browser.close();
    if (cluster) await cluster.stop();
  }
});
