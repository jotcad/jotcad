import nodeFS from 'node:fs';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

// import { nativeNode } from './native.node.cjs';
export const cgal = require('./native.node');

export const FS = {
  stat: (path) => nodeFS.statSync(path),
  readFile: (path, options) => nodeFS.readFileSync(path, options),
  writeFile: (path, data, options) => nodeFS.writeFileSync(path, data, options),
  mkdir: (path) => nodeFS.mkdirSync(path, { recursive: true }),
};
