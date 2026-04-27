import { JotParser } from './parser.js';
import { JotCompiler } from './compiler.js';
import { Selector } from '../../fs/src/vfs_core.js';

/**
 * Registers a VFS provider that evaluates .jot expressions.
 */
export function registerJotProvider(vfs, options = {}) {
  const parser = new JotParser();
  const compiler = options.compiler || new JotCompiler(vfs);

  vfs.registerProvider('jot/eval', async (v, selector, context) => {
    const { expression, params = {} } = selector.parameters;
    if (!expression) throw new Error('[JotProvider] No expression provided to jot/eval');

    const ast = parser.parse(expression);
    const resolved = await compiler.evaluate(ast, params);
    
    if (Array.isArray(resolved)) {
      return {
          type: 'jot/sequence',
          items: resolved
      };
    }

    if (resolved instanceof Selector) {
        return await v.readData(resolved, context);
    }

    return resolved;
  });
}
