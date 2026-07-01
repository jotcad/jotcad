const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 8080;
const IMAGE_DIR = "/home/brian/github/jotcad/sim";

// Strictly limit accessible paths
const ALLOWED_PATHS = {
    "/": "index.html",
    "/index.html": "index.html",
    "/terrain.png": "terrain.png",
    "/terrain_side.png": "terrain_side.png",
    "/terrain_iso.png": "terrain_iso.png"
};

const server = http.createServer((req, res) => {
    let reqPath = req.url;
    if (reqPath.includes('?')) {
        reqPath = reqPath.split('?')[0];
    }
    
    // Dynamic match for permitted images or index.html
    const match = reqPath.match(/^\/(terrain|terrain_side|terrain_iso|terrain_velocity)(?:_(\d+))?\.png$/);
    
    let filename = "";
    if (reqPath === "/" || reqPath === "/index.html") {
        filename = "index.html";
    } else if (match) {
        filename = match[1] + (match[2] ? "_" + match[2] : "") + ".png";
    } else {
        res.writeHead(403, { 'Content-Type': 'text/plain' });
        res.end("403 Forbidden: Access to this resource is restricted.");
        return;
    }
    
    // Serve HTML Dashboard
    if (filename === "index.html") {
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
    
    // Serve Permitted Images
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
});

server.listen(PORT, () => {
    console.log(`Starting secure Node.js web server on port ${PORT}...`);
    console.log(`You can view it in your browser at: http://localhost:${PORT}`);
});
