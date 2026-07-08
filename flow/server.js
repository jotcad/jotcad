import https from 'https';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PORT = 8085;
const HTML_FILE = path.join(__dirname, 'index.html');

// Read the existing SSL certificate files from the workspace
const options = {
    key: fs.readFileSync(path.join(__dirname, '../sim_rivers/key.pem')),
    cert: fs.readFileSync(path.join(__dirname, '../sim_rivers/cert.pem'))
};

const server = https.createServer(options, (req, res) => {
    const cleanUrl = (req.url === '/' || req.url === '' ? '/index.html' : req.url).split('?')[0];
    const safeUrl = cleanUrl.startsWith('/') ? cleanUrl.substring(1) : cleanUrl;
    const filePath = path.resolve(__dirname, safeUrl);
    
    // Prevent directory traversal attacks
    if (!filePath.startsWith(__dirname)) {
        console.error(`[403] Forbidden: ${cleanUrl}`);
        res.writeHead(403, { 'Content-Type': 'text/plain' });
        res.end('Forbidden');
        return;
    }
    
    const ext = path.extname(filePath);
    const mimeTypes = {
        '.html': 'text/html',
        '.css': 'text/css',
        '.js': 'text/javascript',
        '.png': 'image/png',
        '.jpg': 'image/jpeg',
        '.jpeg': 'image/jpeg',
        '.svg': 'image/svg+xml',
        '.json': 'application/json'
    };
    const contentType = mimeTypes[ext] || 'application/octet-stream';

    console.log(`[REQUEST] ${req.method} ${req.url} -> File: ${safeUrl}`);

    fs.readFile(filePath, (err, data) => {
        if (err) {
            console.error(`[404] Not Found: ${safeUrl}`);
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            res.end('Resource not found');
            return;
        }
        console.log(`[200] OK: ${safeUrl} (${data.length} bytes)`);
        res.writeHead(200, { 
            'Content-Type': contentType,
            'Cache-Control': 'no-store, no-cache, must-revalidate, max-age=0'
        });
        res.end(data);
    });
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`\n======================================================`);
    console.log(`HTTPS Server successfully started!`);
    console.log(`View the visualization at: https://localhost:${PORT}`);
    console.log(`======================================================\n`);
});
