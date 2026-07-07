import puppeteer from 'puppeteer';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

async function run() {
    console.log('Launching headless browser...');
    const browser = await puppeteer.launch({
        headless: true,
        args: [
            '--no-sandbox',
            '--disable-setuid-sandbox',
            '--ignore-certificate-errors'
        ]
    });

    try {
        const page = await browser.newPage();
        await page.setCacheEnabled(false);
        
        // Adjust viewport size to match isometric layout aspect ratio
        await page.setViewport({ width: 1200, height: 900 });

        console.log('Navigating to visualizer page...');
        await page.goto('https://localhost:8085/catchment.html', {
            waitUntil: 'networkidle2',
            timeout: 60000
        });

        console.log('Waiting 5s for JS engine to parse 27MB database...');
        await new Promise(r => setTimeout(r, 5000));

        console.log('Waiting for elements to render...');
        await page.waitForSelector('#iso-svg');
        await page.waitForSelector('#show-final-btn');

        console.log('Clicking "Show Final State" button...');
        await page.click('#show-final-btn');

        // Allow some time for SVG elements to fully render and lines to animate
        console.log('Waiting for rendering to finalize...');
        await new Promise(r => setTimeout(r, 1000));

        const outputPath = path.join(__dirname, 'final_iso_render.png');
        console.log(`Taking screenshot of isometric SVG view and saving to ${outputPath}...`);
        
        const isoSvgElement = await page.$('#iso-svg');
        if (!isoSvgElement) {
            throw new Error('Could not find #iso-svg element on the page.');
        }

        await isoSvgElement.screenshot({
            path: outputPath
        });

        console.log('SUCCESS: Final isometric frame captured successfully!');
    } catch (err) {
        console.error('ERROR during visualizer capture:', err);
        process.exit(1);
    } finally {
        await browser.close();
    }
}

run();
