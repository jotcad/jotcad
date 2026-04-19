import puppeteer from 'puppeteer';
import { spawn } from 'child_process';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

async function run() {
    console.log('[Puppeteer] Starting Vite server...');
    const vite = spawn('npm', ['run', 'dev'], { 
        cwd: path.resolve(__dirname, '..'),
        shell: true 
    });

    // Give Vite a moment to start
    await new Promise(r => setTimeout(r, 2000));

    const browser = await puppeteer.launch({
        headless: "new",
        args: ['--no-sandbox', '--disable-setuid-sandbox']
    });

    const page = await browser.newPage();
    await page.setViewport({ width: 512, height: 512 });

    page.on('console', msg => console.log('[Browser Console]', msg.text()));

    console.log('[Puppeteer] Loading test page...');
    await page.goto('http://localhost:3030/test/outline_render.html');

    console.log('[Puppeteer] Waiting for RENDER_COMPLETE...');
    await page.waitForFunction(() => window.RENDER_COMPLETE === true, { timeout: 10000 });

    console.log('[Puppeteer] Taking snapshot...');
    await page.screenshot({ path: path.resolve(__dirname, 'outline_snapshot.png') });
    console.log('[Puppeteer] Snapshot saved to ux/test/outline_snapshot.png');

    await browser.close();
    vite.kill();
    process.exit(0);
}

run().catch(err => {
    console.error(err);
    process.exit(1);
});
