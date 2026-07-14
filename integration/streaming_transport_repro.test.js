import assert from 'node:assert';
import http from 'node:http';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Theory: ReadableStream body triggers Chunked Encoding on HTTP/1.1', async ({ t }) => {
    let receivedEncoding = '';
    let receivedContentLength = '';
    
    // 1. Create a standard HTTP/1.1 Server
    const server = http.createServer((req, res) => {
        receivedEncoding = req.headers['transfer-encoding'];
        receivedContentLength = req.headers['content-length'];
        
        req.on('data', () => {});
        req.on('end', () => {
            res.writeHead(200);
            res.end('ok');
        });
    });

    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const port = server.address().port;
    const url = `http://127.0.0.1:${port}`;

    try {
        // 2. Create a ReadableStream (Web Stream API, used in browser)
        const stream = new ReadableStream({
            start(controller) {
                controller.enqueue(new TextEncoder().encode('chunk1'));
                controller.enqueue(new TextEncoder().encode('chunk2'));
                controller.close();
            }
        });

        console.log('[Test] Attempting streaming fetch to HTTP/1.1 server...');
        
        // 3. Perform the fetch with the stream body
        const resp = await fetch(url, {
            method: 'POST',
            body: stream,
            // Node/undici specific: duplex must be set for streaming bodies
            duplex: 'half' 
        });

        const text = await resp.text();
        console.log('[Test] Response:', text);

        // 4. Verify the headers
        console.log('[Test] Transfer-Encoding:', receivedEncoding);
        console.log('[Test] Content-Length:', receivedContentLength);

        // In HTTP/1.1, if it's a stream, it MUST be chunked and MUST NOT have content-length
        assert.strictEqual(receivedEncoding, 'chunked');
        assert.strictEqual(receivedContentLength, undefined);

        console.log('[Test] THEORY CONFIRMED: ReadableStream forces Chunked Encoding.');
        console.log('[Note] Chrome specifically blocks this "Streaming Upload" behavior over HTTP/1.1, requiring HTTP/2.');

    } finally {
        server.close();
    }
});
