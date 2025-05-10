import '@babel/plugin-syntax-bigint';

import babel from '@rollup/plugin-babel';
import builtins from 'rollup-plugin-node-builtins';
import cjs from 'rollup-plugin-cjs-es';
import commonjs from '@rollup/plugin-commonjs';
import hypothetical from 'rollup-plugin-hypothetical';
import nodePolyfills from 'rollup-plugin-node-polyfills';
import nodeResolve from '@rollup/plugin-node-resolve';

Error.stackTraceLimit = Infinity;

export default {
  treeshake: false,
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
        './getCgal.js': `export { cgal, cgalIsReady, getCgal, initCgal } from './getWasmCgal.js';`,
      },
    }),
    // builtins(),
    // commonjs(),
    {
      transform(code, id) {
        return code.replace(/'@jotcad\/([^']*)'/g, "'./jotcad-$1.js'");
      },
    },
    /*
    {
      transform ( code, id ) {
        console.log( id );
        console.log( code );
        // not returning anything, so doesn't affect bundle
      }
    },
*/
    nodeResolve({ preferBuiltins: false }),
    // nodePolyfills(),
    cjs(),
    // nodeResolve({ preferBuiltins: true }),
  ],
};
