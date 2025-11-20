import * as api from './api.js';

import {
  cleanupSessions,
  getOrCreateSession,
  startCleanup,
} from './session.js'; // Added cleanupSessions
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
import vm from 'node:vm';
import { withAssets } from '@jotcad/geometry';
import { withFs } from '@jotcad/ops';

const bindings = { ...api, note, view };

const whitelist = {
  functions: [
    'And',
    'Arc2',
    'Box2',
    'Box3',
    'Jot',
    'Orb',
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
    'jot',
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
    'Orb',
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
    'jot',
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
  nodes: [
    'Program',
    'ExpressionStatement',
    'CallExpression',
    'Identifier',
    'Literal',
    'MemberExpression',
  ],
  operators: ['+', '-', '*', '/', '='],
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
      `Path Traversal Detected: "${targetPaths}" attempts to escape base directory "${absoluteBase}"`
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
    case '.jot':
      return 'text/plain';
    default:
      return 'application/octet-stream';
  }
};

const handleGet = async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const [op, sessionId, ...rest] = url.pathname.split('/').slice(1);

    if (!sessionId) {
      res.writeHead(400, { 'Content-Type': 'text/plain' });
      res.end('Session ID is required.');
      return;
    }

    if (sessionId === 'favicon.ico') {
      throw Error('favicon');
    }

    const session = await getOrCreateSession(sessionId);

    if (op === 'run') {
      const [code, downloadFile] = rest;
      const ecmascript = compile(decodeURIComponent(code), whitelist);
      console.log(`Run ${ecmascript}`);

      const context = vm.createContext({ ...bindings });
      const script = new vm.Script(ecmascript);
      const evaluator = () => script.runInContext(context, { timeout: 5000 });

      const fs = {
        writeFile: async (file, data) => {
          const path = joinPaths(session.filesPath, file);
          await writeFile(path, data);
        },
      };

      await withAssets(session.assetsPath, async (assets) => {
        await withFs(fs, async () => {
          await run(assets, () => evaluator(bindings));
        });
      });

      if (downloadFile) {
        const downloadPath = joinPaths(session.filesPath, downloadFile);
        const stats = await stat(downloadPath);
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
      } else {
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end('ok');
      }
    } else if (op === 'get') {
      const [downloadFile] = rest;
      const downloadPath = joinPaths(session.filesPath, downloadFile);
      const stats = await stat(downloadPath);
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
    } else {
      res.writeHead(400, { 'Content-Type': 'text/plain' });
      res.end('Invalid operation. Use "run" or "get".');
    }
  } catch (e) {
    console.log(`QQ/error: ${e}`);
    res.writeHead(500, { 'Content-Type': 'text/plain' });
    res.end('' + e);
  }
};

const handlePost = async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const [op, sessionId, downloadFile] = url.pathname.split('/').slice(1);

    if (op !== 'run') {
      res.writeHead(400, { 'Content-Type': 'text/plain' });
      res.end('Invalid operation. Use "run".');
      return;
    }

    if (!sessionId) {
      res.writeHead(400, { 'Content-Type': 'text/plain' });
      res.end('Session ID is required.');
      return;
    }

    const session = await getOrCreateSession(sessionId);
    let body = '';
    for await (const chunk of req) {
      body += chunk;
    }
    const ecmascript = compile(body, whitelist);
    console.log(`Run ${ecmascript}`);

    const context = vm.createContext({ ...bindings });
    const script = new vm.Script(ecmascript);
    const evaluator = () => script.runInContext(context, { timeout: 5000 });

    const fs = {
      writeFile: async (file, data) => {
        const path = joinPaths(session.filesPath, file);
        await writeFile(path, data);
      },
    };

    await withAssets(session.assetsPath, async (assets) => {
      await withFs(fs, async () => {
        await run(assets, () => evaluator(bindings));
      });
    });

    if (downloadFile) {
      const downloadPath = joinPaths(session.filesPath, downloadFile);
      const stats = await stat(downloadPath);
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
    } else {
      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end('ok');
    }
  } catch (e) {
    console.error(`Error handling POST request: ${e}`);
    res.writeHead(500, { 'Content-Type': 'text/plain' });
    res.end('' + e);
  }
};

const hostname = process.env.HOSTNAME || '127.0.0.1';
const port = process.env.PORT ? parseInt(process.env.PORT, 10) : 3000;

const server = http.createServer((req, res) => {
  switch (req.method) {
    case 'GET': {
      handleGet(req, res);
      break;
    }
    case 'POST': {
      handlePost(req, res);
      break;
    }
    default: {
      res.writeHead(405, { 'Content-Type': 'text/plain' });
      res.end('Method Not Allowed');
    }
  }
});

// Check for --cleanup-and-exit argument
if (process.argv.includes('--cleanup-and-exit')) {
  console.log('Cleanup and exit requested. Performing session cleanup...');
  await cleanupSessions(); // Call cleanupSessions directly
  console.log('Session cleanup complete. Exiting.');
  process.exit(0);
} else {
  server.listen(port, hostname, () => {
    console.log(`Server running at http://${hostname}:${port}/`);
    startCleanup();
  });
}
