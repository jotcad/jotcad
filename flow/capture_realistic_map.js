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
        
        // Large viewport to fit panels
        await page.setViewport({ width: 1600, height: 1200 });

        console.log('Navigating to realistic map visualizer page...');
        await page.goto('https://localhost:8085/realistic_map.html', {
            waitUntil: 'networkidle2',
            timeout: 60000
        });

        console.log('Waiting 5s for JS engine to parse database...');
        await page.waitForTimeout ? await page.waitForTimeout(5000) : await new Promise(r => setTimeout(r, 5000));

        console.log('Jumping to final frame on timeline...');
        await page.evaluate(() => {
            const slider = document.getElementById('timeline-slider');
            if (slider) {
                slider.value = slider.max;
                slider.dispatchEvent(new Event('input'));
            }
        });

        console.log('Waiting 2s for rendering to finalize...');
        await page.waitForTimeout ? await page.waitForTimeout(2000) : await new Promise(r => setTimeout(r, 2000));

        // 1. Capture 2D Overhead Map (#grid-svg)
        console.log('Capturing 2D Overhead Topographic Map (#grid-svg)...');
        const gridSvgElement = await page.$('#grid-svg');
        if (gridSvgElement) {
            const outputPath2D = path.join(__dirname, 'realistic_map_2d.png');
            await gridSvgElement.screenshot({ path: outputPath2D });
            console.log(`Saved 2D snapshot to ${outputPath2D}`);
        } else {
            console.error('Could not find #grid-svg element');
        }

        // 2. Capture 3D Isometric View (#iso-svg)
        console.log('Capturing 3D Isometric Relief Model (#iso-svg)...');
        const isoSvgElement = await page.$('#iso-svg');
        if (isoSvgElement) {
            const outputPath3D = path.join(__dirname, 'realistic_map_3d.png');
            await isoSvgElement.screenshot({ path: outputPath3D });
            console.log(`Saved 3D snapshot to ${outputPath3D}`);
        } else {
            console.error('Could not find #iso-svg element');
        }

        console.log('SUCCESS: Separate 2D and 3D terrain renders captured successfully!');

    } catch (e) {
        console.error('ERROR during capture:', e);
    } finally {
        await browser.close();
    }
}

run();
