import https from 'https';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PORT = 8080;
const IMAGE_DIR = "/home/brian/github/jotcad/sim_rivers";

// Strictly limit accessible paths
const ALLOWED_PATHS = {
    "/": "index.html",
    "/index.html": "index.html",
    "/terrain.png": "terrain.png",
    "/terrain_side.png": "terrain_side.png",
    "/terrain_iso.png": "terrain_iso.png"
};

const options = {
    key: fs.readFileSync(path.join(IMAGE_DIR, 'key.pem')),
    cert: fs.readFileSync(path.join(IMAGE_DIR, 'cert.pem'))
};

const server = https.createServer(options, (req, res) => {
    let reqPath = req.url;
    if (reqPath.includes('?')) {
        reqPath = reqPath.split('?')[0];
    }
    
    // Serve HTML Dashboard
    if (reqPath === "/" || reqPath === "/index.html") {
        fs.readFile(path.join(IMAGE_DIR, "index.html"), "utf8", (err, content) => {
            if (err) {
                res.writeHead(500, { 'Content-Type': 'text/plain' });
                res.end("500 Internal Server Error");
                return;
            }
            res.writeHead(200, {
                'Content-Type': 'text/html; charset=utf-8',
                'Cache-Control': 'no-store, no-cache, must-revalidate'
            });
            res.end(content);
        });
        return;
    }
    
    // Dynamic match for permitted images (e.g., terrain_100.png or terrain.png)
    const match = reqPath.match(/^\/(terrain|terrain_side|terrain_iso|terrain_velocity)(?:_(\d+))?\.png$/);
    if (match) {
        const type = match[1];
        const step = match[2];
        const filename = step ? `${type}_${step}.png` : `${type}.png`;
        const filepath = path.join(IMAGE_DIR, filename);
        
        fs.access(filepath, fs.constants.F_OK, (err) => {
            if (err) {
                res.writeHead(404, { 'Content-Type': 'text/plain' });
                res.end("404 Not Found");
                return;
            }
            
            res.writeHead(200, {
                'Content-Type': 'image/png',
                'Cache-Control': 'no-store, no-cache, must-revalidate'
            });
            fs.createReadStream(filepath).pipe(res);
        });
        return;
    }
    
    res.writeHead(403, { 'Content-Type': 'text/plain' });
    res.end("403 Forbidden: Access to this resource is restricted.");
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`Starting secure Node.js ES6 HTTPS web server on port ${PORT} (all interfaces)...`);
    console.log(`You can view it in your browser at: https://localhost:${PORT}`);
});
