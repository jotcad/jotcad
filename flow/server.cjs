const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 8080;
const HTML_FILE = path.join(__dirname, 'index.html');

const server = http.createServer((req, res) => {
    // Only serve index.html for simplicity
    fs.readFile(HTML_FILE, (err, data) => {
        if (err) {
            res.writeHead(500, { 'Content-Type': 'text/plain' });
            res.end('Error loading index.html');
            return;
        }
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(data);
    });
});

server.listen(PORT, () => {
    console.log(`\n======================================================`);
    console.log(`Server successfully started!`);
    console.log(`View the visualization at: http://localhost:${PORT}`);
    console.log(`======================================================\n`);
});
