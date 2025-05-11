import builtins from 'rollup-plugin-node-builtins';
import commonjs from '@rollup/plugin-commonjs';
import globals from 'rollup-plugin-node-globals';
import hypothetical from 'rollup-plugin-hypothetical';
import nodeResolve from '@rollup/plugin-node-resolve';

Error.stackTraceLimit = Infinity;

export default {
  input: 'main.js',
  output: {
    dir: 'dist',
    format: 'module',
  },
  external(id) {
    return id.startsWith('./jotcad-');
  },
  plugins: [
    hypothetical({
      allowFallthrough: true,
      allowRealFiles: true,
      files: {
        './fs.js': `export { readFile, writeFile } from './fsWeb.js';`,
      },
    }),
    builtins(),
    commonjs(),
    globals(),
    nodeResolve({ preferBuiltins: true }),
    {
      transform(code, _id) {
        return code.replace(/'@jotcad\/([^']*)'/g, "'./jotcad-$1.js'");
      },
    },
  ],
};
