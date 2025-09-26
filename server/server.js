import * as api from './api.js';
import { cgal, withAssets } from '@jotcad/geometry';
import { mkdir, readFile, stat, writeFile } from 'node:fs/promises';
import { URL } from 'url';
import { compile } from './compiler.js';
import { createReadStream } from 'node:fs';
import hash from 'string-hash';
import http from 'http';
import { note } from './note.js';
import path from 'path';
import { run } from '@jotcad/op';
import { view } from './view.js';
import { withFs } from '@jotcad/ops';

const bindings = { ...api, note, view };

const whitelist = {
  functions: [
    'And',
    'Arc2',
    'Box2',
    'Point',
    'Z',
    'absolute',
    'and',
    'at',
    'color',
    'clip',
    'cut',
    'extrude2',
    'extrude3',
    'fill2',
    'fill3',
    'get',
    'join',
    'nth',
    'mask',
    'png',
    'revert',
    'rx',
    'ry',
    'rz',
    'set',
    'simplify',
    'stl',
    'test',
    'transform',
    'x',
    'y',
    'z',
  ],
  methods: [
    'And',
    'Arc2',
    'Box2',
    'Point',
    'Z',
    'absolute',
    'and',
    'at',
    'color',
    'clip',
    'cut',
    'extrude2',
    'extrude3',
    'fill2',
    'fill3',
    'get',
    'join',
    'nth',
    'mask',
    'png',
    'revert',
    'rx',
    'ry',
    'rz',
    'set',
    'simplify',
    'stl',
    'test',
    'transform',
    'x',
    'y',
    'z',
  ],
  globals: [],
};

Error.stackTraceLimit = Infinity;

process.on('uncaughtException', (err) => {
  console.error('There was an uncaught error', err, err.stack);
  process.exit(1); // mandatory (as per the Node.js docs)
});

const joinPaths = (baseDir, ...targetPaths) => {
  const absoluteBase = path.resolve(baseDir);
  const absoluteTarget = path.resolve(absoluteBase, ...targetPaths);
  if (
    !absoluteTarget.startsWith(absoluteBase + path.sep) &&
    absoluteTarget !== absoluteBase
  ) {
    throw new Error(
      `Path Traversal Detected: "${targetPath}" attempts to escape base directory "${absoluteBase}"`
    );
  }
  return absoluteTarget;
};

const getContentType = (filePath) => {
  const ext = path.extname(filePath);
  switch (ext) {
    case '.html':
      return 'text/html';
    case '.css':
      return 'text/css';
    case '.js':
      return 'text/javascript';
    case '.json':
      return 'application/json';
    case '.png':
      return 'image/png';
    case '.jpg':
    case '.jpeg':
      return 'image/jpeg';
    default:
      return 'application/octet-stream';
  }
};

const handleGet = async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const [op, code, downloadFile] = url.pathname.split('/');
    if (code === 'favicon.ico') {
      throw Error('favicon');
    }
    const ecmascript = compile(decodeURIComponent(code), whitelist);
    console.log(`Run ${ecmascript} ${downloadFile}`);

    const evaluator = new Function(
      `{ ${Object.keys({ ...bindings }).join(', ')} }`,
      ecmascript
    );

    let output = 'ok';
    let mimeType = 'text/plain';

    const basePath = `./io/${hash(ecmascript)}`;
    const downloadPath = joinPaths(basePath, downloadFile);

    const fs = {
      writeFile: async (file, data) => {
        // Write our session artifacts.
        const path = joinPaths(basePath, file);
        await writeFile(path, data);
        if (file === path) {
          output = data;
          if (file.endsWith('.png')) {
            mimeType = 'image/png';
          } else if (file.endsWith('.svg')) {
            mimeType = 'image/svg+xml';
          }
        }
      },
    };

    await mkdir(basePath, { recursive: true });

    let stats;
    try {
      stats = await stat(downloadPath);
    } catch (e) {
      if (e.code === 'ENOENT') {
        await withAssets(async (assets) => {
          await withFs(fs, async () => {
            const graph = await run(assets, () => evaluator(bindings));
          });
        });
      }
      stats = await stat(downloadPath);
    }

    const contentType = getContentType(downloadPath);

    const readStream = createReadStream(downloadPath);

    res.writeHead(200, {
      'Content-Type': contentType,
      'Content-Length': stats.size,
    });

    readStream.pipe(res);

    readStream.on('error', (streamErr) => {
      console.error('Stream Error:', streamErr);
      if (!res.headersSent) {
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end('Server error.');
      } else {
        res.end();
      }
    });
    return;
  } catch (e) {
    console.log(`QQ/error: ${e}`);
    res.writeHead(500, { 'Content-Type': 'text/plain' });
    res.end('' + e);
  }
};

const hostname = '127.0.0.1';
const port = 3000;

const server = http.createServer((req, res) => {
  switch (req.method) {
    case 'GET': {
      handleGet(req, res);
    }
  }
});

server.listen(port, hostname, () => {
  console.log(`Server running at http://${hostname}:${port}/`);
});
