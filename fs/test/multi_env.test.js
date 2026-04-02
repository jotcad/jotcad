import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { registerVFSRoutes } from '../src/vfs_rest_server.js';
import { Readable } from 'stream';
import puppeteer from 'puppeteer';
import http from 'http';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('Multi-Environment VFS (REST + SSE)', { timeout: 20000 }, async (t) => {
  const vfs = new VFS({ id: 'node' });

  // 1. Setup HTTP Server
  const server = http.createServer((req, res) => {
    let filePath;
    if (req.url === '/' || req.url === '/index.html') {
      filePath = path.join(__dirname, 'web', 'index.html');
    } else if (req.url.startsWith('/src/')) {
      filePath = path.join(
        __dirname,
        '..',
        'src',
        req.url.replace('/src/', '')
      );
    } else {
      filePath = path.join(__dirname, 'web', req.url);
    }

    if (fs.existsSync(filePath) && fs.statSync(filePath).isFile()) {
      const ext = path.extname(filePath);
      const contentType =
        ext === '.js' ? 'application/javascript' : 'text/html';
      res.writeHead(200, {
        'Content-Type': contentType,
        'Cross-Origin-Opener-Policy': 'same-origin',
        'Cross-Origin-Embedder-Policy': 'require-corp',
      });
      fs.createReadStream(filePath).pipe(res);
    } else {
      // Let the VFS routes handle it if it didn't match a file
    }
  });

  // 2. Attach VFS REST Endpoints
  registerVFSRoutes(vfs, server, '/vfs');

  const port = await new Promise((resolve) =>
    server.listen(0, '127.0.0.1', () => resolve(server.address().port))
  );
  const baseUrl = `http://127.0.0.1:${port}`;

  // 3. Launch Puppeteer
  const browser = await puppeteer.launch({
    headless: 'new',
    args: ['--no-sandbox', '--disable-setuid-sandbox'],
  });
  const page = await browser.newPage();

  page.on('console', (msg) => console.log(`[Browser] ${msg.text()}`));
  page.on('pageerror', (err) =>
    console.error(`[Browser Error] ${err.message}`)
  );

  await page.goto(baseUrl);

  await t.test(
    'cascading coordination via REST + SSE',
    { timeout: 10000 },
    async () => {
      console.log('[Node] Triggering Web Worker...');
      await vfs.tickle('trigger-worker');

      console.log('[Node] Waiting for Service Worker result (sw-done)...');
      const swDoneStream = await vfs.read('sw-done');

      let swData = '';
      const decoder = new TextDecoder();
      for await (const chunk of swDoneStream) {
        swData += decoder.decode(chunk, { stream: true });
      }
      swData += decoder.decode(); // Flush
      assert.strictEqual(swData, 'Hello from SW');
      console.log('[Node] Service Worker finished!');

      console.log('[Node] Writing final result...');
      await vfs.write(
        'final-result',
        {},
        Readable.from([Buffer.from('Success!')])
      );

      await page.waitForFunction(
        () => {
          const el = document.getElementById('status');
          return el && el.textContent === 'Test Passed';
        },
        { timeout: 15000 }
      );

      const status = await page.$eval('#status', (el) => el.textContent);
      assert.strictEqual(status, 'Test Passed');
    }
  );

  // Cleanup
  await browser.close();
  server.close();
  await vfs.close();
});
