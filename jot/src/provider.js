import { JotParser } from './parser.js';
import { JotCompiler } from './compiler.js';

/**
 * Registers a VFS provider that evaluates .jot expressions.
 */
export function registerJotProvider(vfs, options = {}) {
  const parser = new JotParser();
  const compiler = options.compiler || new JotCompiler(vfs);

  vfs.registerProvider('jot/eval', async (v, selector, context) => {
    const { expression, params = {} } = selector.parameters;
    if (!expression) return null;

    try {
      const ast = parser.parse(expression);
      const resolved = await compiler.evaluate(ast, params);
      if (Array.isArray(resolved)) {
        return {
            type: 'jot/sequence',
            items: resolved
        };
      }
      return await v.read(resolved.path, resolved.parameters, context);
    } catch (err) {
      console.error(`[JotProvider] Evaluation error:`, err);
      return null;
    }
  });

  vfs.registerProvider('.jot', async (v, selector, context) => {
    if (!selector.path.endsWith('.jot')) return null;
    try {
      const expression = await v.readText(selector.path, {}, context);
      if (!expression) return null;
      return await v.read(
        'jot/eval',
        { expression, params: selector.parameters },
        context
      );
    } catch (err) {
      console.error(`[JotProvider] File error for ${selector.path}:`, err);
      return null;
    }
  });
}
