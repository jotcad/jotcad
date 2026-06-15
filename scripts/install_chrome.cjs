const { install, Browser, detectBrowserPlatform } = require('@puppeteer/browsers');
const path = require('path');
const os = require('os');

async function main() {
  const platform = detectBrowserPlatform();
  console.log('Detected platform:', platform);
  const cacheDir = path.join(os.homedir(), '.cache', 'puppeteer');
  console.log('Installing Chrome to:', cacheDir);
  const result = await install({
    browser: Browser.CHROME,
    buildId: '148.0.7778.97',
    cacheDir: cacheDir,
    platform: platform
  });
  console.log('Chrome download complete!', result);
}

main().catch(err => {
  console.error('Download failed:', err);
  process.exit(1);
});
