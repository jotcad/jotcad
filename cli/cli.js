import * as api from '@jotcad/ops';

import { cgal, withAssets } from '@jotcad/geometry';

import { argv } from 'process';
import { note } from './note.js';
import { readFile } from '@jotcad/ops';
import { run } from '@jotcad/op';
import { testPng } from './testPng.js';
import { view } from './view.js';

Error.stackTraceLimit = Infinity;

process.on('uncaughtException', (err) => {
  console.error('There was an uncaught error', err, err.stack);
  process.exit(1); // mandatory (as per the Node.js docs)
});

const cli = async (scriptPath, ...args) => {
  const outputMap = new Map();
  const bindings = { ...api, view };
  args.forEach((arg, index) => {
    bindings[`$${index + 1}`] = arg;
  });
  const cwd = process.cwd();
  const script = await readFile(scriptPath, 'utf8');
  const lines = script.split('\n');
  const ecmascript = lines.slice(1).join('\n');
  const evaluator = new Function(
    `{ ${Object.keys({ ...bindings }).join(', ')} }`,
    ecmascript
  );
  return withAssets(async (assets) => {
    const graph = await run(assets, () => evaluator(bindings));
  });
};

cli(...argv.slice(2));
