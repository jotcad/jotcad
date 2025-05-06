import * as api from './main.js';

import { cgal, cgalIsReady, shape } from '@jotcad/geometry';
import { readFileSync, writeFileSync } from 'fs';

import { argv } from 'process';
import { run } from '@jotcad/op';

Error.stackTraceLimit = Infinity;

process.on('uncaughtException', (err) => {
  console.error('There was an uncaught error', err);
  process.exit(1); // mandatory (as per the Node.js docs)
});

const cli = async (scriptPath, ...args) => {
  const outputMap = new Map();
  const bindings = { ...api };
  args.forEach((arg, index) => {
    bindings[`$${index + 1}`] = arg;
  });
  const cwd = process.cwd();
  const script = readFileSync(scriptPath, 'utf8');
  const lines = script.split('\n');
  const ecmascript = lines.slice(1).join('\n');
  const assets = { text: {} };
  const evaluator = new Function(`{ ${Object.keys({ ...bindings }).join(', ')} }`, ecmascript);
  await cgalIsReady;
  const graph = await run(assets, () => evaluator(bindings));
};

cli(...argv.slice(2));
