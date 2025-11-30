import * as api from './api.js';

import {
  cleanupSessions,
  getOrCreateSession,
  startCleanup,
} from './session.js'; // Added cleanupSessions
import { constructors, operators } from '../ops/main.js';
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

const bindings = { ...api, note, view };

const whitelist = {
  functions: [...constructors, ...operators],
  methods: operators,
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

const corsHeaders = {
  'Access-Control-Allow-Origin': 'http://127.0.0.1:8080',
  'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type',
};

const log = (message) => {
  console.log(`Jot: ${message}`);
};

const sendResponse = (res, statusCode, headers, body) => {
  log(`Sending response: ${statusCode}`);
  res.writeHead(statusCode, { ...corsHeaders, ...headers });
  res.end(body);
};

const sendStream = (
  res,
  statusCode,
  headers,
  stream,
  filePath,
  contentType,
  contentLength
) => {
  log(
    `Sending file: ${filePath}, Content-Type: ${contentType}, Content-Length: ${contentLength}`
  );
  res.writeHead(statusCode, { ...corsHeaders, ...headers });
  stream.pipe(res);
};

const handleOptions = (req, res) => {
  sendResponse(res, 200, {});
};

const handleGet = async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const [op, sessionId, ...rest] = url.pathname.split('/').slice(1);

    if (!sessionId) {
      sendResponse(
        res,
        400,
        { 'Content-Type': 'text/plain' },
        'Session ID is required.'
      );
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

      await withAssets(session.paths.assets, async (assets) => {
        await run(session, () => evaluator(bindings));
      });

      if (downloadFile) {
        const downloadPath = joinPaths(session.paths.files, downloadFile);
        const stats = await stat(downloadPath);
        const contentType = getContentType(downloadPath);
        const readStream = createReadStream(downloadPath);

        sendStream(
          res,
          200,
          {
            'Content-Type': contentType,
            'Content-Length': stats.size,
          },
          readStream,
          downloadPath,
          contentType,
          stats.size
        );

        readStream.on('error', (streamErr) => {
          console.error('Stream Error:', streamErr);
          if (!res.headersSent) {
            sendResponse(
              res,
              500,
              { 'Content-Type': 'text/plain' },
              `Stream error: ${streamErr.message}`
            );
          } else {
            res.end();
          }
        });
      } else {
        sendResponse(res, 200, { 'Content-Type': 'text/plain' }, 'ok');
      }
    } else if (op === 'get') {
      const [downloadFile] = rest;
      const downloadPath = joinPaths(session.paths.files, downloadFile);
      const stats = await stat(downloadPath);
      const contentType = getContentType(downloadPath);
      const readStream = createReadStream(downloadPath);

      sendStream(
        res,
        200,
        {
          'Content-Type': contentType,
          'Content-Length': stats.size,
        },
        readStream,
        downloadPath,
        contentType,
        stats.size
      );

      readStream.on('error', (streamErr) => {
        console.error('Stream Error:', streamErr);
        if (!res.headersSent) {
          sendResponse(
            res,
            500,
            { 'Content-Type': 'text/plain' },
            `Stream error: ${streamErr.message}`
          );
        } else {
          res.end();
        }
      });
    } else {
      sendResponse(
        res,
        400,
        { 'Content-Type': 'text/plain' },
        'Invalid operation. Use "run" or "get".'
      );
    }
  } catch (e) {
    console.error(`Error handling GET request: ${e}\n${e.stack}`);
    sendResponse(res, 500, { 'Content-Type': 'text/plain' }, '' + e);
  }
};

const handlePost = async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const [op, sessionId, downloadFile] = url.pathname.split('/').slice(1);

    if (op !== 'run') {
      sendResponse(
        res,
        400,
        { 'Content-Type': 'text/plain' },
        'Invalid operation. Use "run".'
      );
      return;
    }

    if (!sessionId) {
      sendResponse(
        res,
        400,
        { 'Content-Type': 'text/plain' },
        'Session ID is required.'
      );
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

    await withAssets(session.paths.assets, async (assets) => {
      await run(session, () => evaluator(bindings));
    });

    if (downloadFile) {
      const downloadPath = joinPaths(session.paths.files, downloadFile);
      const stats = await stat(downloadPath);
      const contentType = getContentType(downloadPath);
      const readStream = createReadStream(downloadPath);

      sendStream(
        res,
        200,
        {
          'Content-Type': contentType,
          'Content-Length': stats.size,
        },
        readStream,
        downloadPath,
        contentType,
        stats.size
      );

      readStream.on('error', (streamErr) => {
        console.error('Stream Error:', streamErr);
        if (!res.headersSent) {
          sendResponse(
            res,
            500,
            { 'Content-Type': 'text/plain' },
            `Stream error: ${streamErr.message}`
          );
        } else {
          res.end();
        }
      });
    } else {
      sendResponse(res, 200, { 'Content-Type': 'text/plain' }, 'ok');
    }
  } catch (e) {
    console.error(`Error handling POST request: ${e}\n${e.stack}`);
    sendResponse(res, 500, { 'Content-Type': 'text/plain' }, '' + e);
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
    case 'OPTIONS': {
      handleOptions(req, res);
      break;
    }
    case 'POST': {
      handlePost(req, res);
      break;
    }
    default: {
      sendResponse(
        res,
        405,
        { 'Content-Type': 'text/plain' },
        'Method Not Allowed'
      );
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
