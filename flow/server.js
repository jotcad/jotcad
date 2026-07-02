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
    let urlPath = req.url === '/' || req.url === '' ? '/index.html' : req.url;
    
    // Sanitize path to prevent directory traversal
    const baseName = path.basename(urlPath);
    const ext = path.extname(baseName);
    const filePath = path.join(__dirname, baseName);
    
    let contentType = 'text/html';
    if (ext === '.js') {
        contentType = 'text/javascript';
    }

    console.log(`[REQUEST] ${req.method} ${req.url} -> File: ${baseName}`);

    fs.readFile(filePath, (err, data) => {
        if (err) {
            console.error(`[404] Not Found: ${baseName}`);
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            res.end('Resource not found');
            return;
        }
        console.log(`[200] OK: ${baseName} (${data.length} bytes)`);
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
