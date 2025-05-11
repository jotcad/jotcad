import '@babel/plugin-syntax-bigint';

import babel from '@rollup/plugin-babel';
import cjs from 'rollup-plugin-cjs-es';
import hypothetical from 'rollup-plugin-hypothetical';
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
        gl: 'const dummy = {}; export default dummy;',
        './getCgal.js': "export { cgal, cgalIsReady } from './getWasmCgal.js';",
      },
    }),
    {
      transform(code, id) {
        return code.replace(/'@jotcad\/([^']*)'/g, "'./jotcad-$1.js'");
      },
    },
    nodeResolve({ preferBuiltins: false }),
    cjs(),
  ],
};
