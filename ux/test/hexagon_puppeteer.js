import puppeteer from 'puppeteer';
import { exec } from 'child_process';
import path from 'path';

async function runTest() {
    console.log('[Puppeteer] Starting Vite server...');
    // Start vite in the background
    const vite = exec('npx vite --port 3031 --strictPort', { cwd: path.join(process.cwd(), 'ux') });
    
    // Wait for vite to start
    await new Promise(resolve => setTimeout(resolve, 3000));

    const browser = await puppeteer.launch({
        headless: "new",
        args: ['--no-sandbox', '--disable-setuid-sandbox']
    });

    try {
        const page = await browser.newPage();
        await page.setViewport({ width: 800, height: 600 });
        
        page.on('console', msg => console.log('[Browser Console]', msg.text()));

        console.log('[Puppeteer] Loading test page...');
        await page.goto('http://localhost:3031/test/hexagon_render.html');

        console.log('[Puppeteer] Waiting for RENDER_COMPLETE...');
        await page.waitForFunction(() => window.RENDER_COMPLETE === true, { timeout: 10000 });

        console.log('[Puppeteer] Taking snapshot...');
        await page.screenshot({ path: 'ux/test/hexagon_snapshot.png' });
        console.log('[Puppeteer] Snapshot saved to ux/test/hexagon_snapshot.png');

    } catch (err) {
        console.error('[Puppeteer] Error during test:', err);
    } finally {
        await browser.close();
        vite.kill();
        process.exit(0);
    }
}

runTest().catch(console.error);
