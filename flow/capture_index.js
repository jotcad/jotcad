import puppeteer from 'puppeteer';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

async function run() {
    console.log('Launching headless browser...');
    const browser = await puppeteer.launch({
        headless: 'new',
        args: [
            '--no-sandbox',
            '--disable-setuid-sandbox',
            '--ignore-certificate-errors'
        ]
    });

    try {
        const page = await browser.newPage();
        await page.setCacheEnabled(false);
        
        // Large viewport to fit the index gallery
        await page.setViewport({ width: 1400, height: 1200 });

        console.log('Navigating to simulation gallery index page...');
        await page.goto('https://localhost:8085/index.html', {
            waitUntil: 'networkidle2',
            timeout: 60000
        });

        console.log('Waiting 3s for images to load...');
        await page.waitForTimeout ? await page.waitForTimeout(3000) : await new Promise(r => setTimeout(r, 3000));

        const outputPath = path.join(__dirname, 'gallery_index_render.png');
        console.log(`Taking screenshot and saving to ${outputPath}...`);
        await page.screenshot({ path: outputPath, fullPage: true });
        console.log('SUCCESS: Gallery index rendering captured successfully!');

    } catch (e) {
        console.error('ERROR during capture:', e);
    } finally {
        await browser.close();
    }
}

run();
