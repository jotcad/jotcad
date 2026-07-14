import puppeteer from 'puppeteer';
import path from 'path';

async function run() {
    console.log("Launching headless browser...");
    const browser = await puppeteer.launch({
        headless: true,
        ignoreHTTPSErrors: true,
        args: [
            '--no-sandbox', 
            '--disable-setuid-sandbox',
            '--ignore-certificate-errors'
        ]
    });
    const page = await browser.newPage();
    await page.setViewport({ width: 1400, height: 1600 }); // Taller viewport for vertically stacked cards
    
    console.log("Navigating to https://localhost:8085/hex_visualizer.html...");
    await page.goto('https://localhost:8085/hex_visualizer.html', { 
        waitUntil: 'networkidle2',
        timeout: 15000 
    });
    
    console.log("Waiting for loading screen overlay to fade...");
    await new Promise(resolve => setTimeout(resolve, 3500));
    
    console.log("Simulating dragging 2D and 3D sliders to year 100...");
    await page.evaluate(() => {
        const slider2d = document.getElementById('step-slider-2d');
        const slider3d = document.getElementById('step-slider-3d');
        if (slider2d) {
            slider2d.value = 100;
            slider2d.dispatchEvent(new Event('input'));
        }
        if (slider3d) {
            slider3d.value = 100;
            slider3d.dispatchEvent(new Event('input'));
        }
    });
    
    console.log("Waiting for images and overlays to update...");
    await new Promise(resolve => setTimeout(resolve, 1500));
    
    console.log("Taking screenshot of the vertically stacked layout...");
    const screenshotPath = '/home/brian/github/jotcad/flow/hex_screenshot.png';
    await page.screenshot({ path: screenshotPath });
    
    console.log(`SUCCESS: Saved screenshot to ${screenshotPath}`);
    await browser.close();
}

run().catch(err => {
    console.error("Puppeteer script execution failed:", err);
    process.exit(1);
});
