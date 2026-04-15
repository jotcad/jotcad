import { JotParser } from './parser.js';
import { JotCompiler } from './compiler.js';

/**
 * Registers a VFS provider that evaluates .jot expressions.
 */
export function registerJotProvider(vfs, options = {}) {
  const parser = new JotParser();
  const compiler = options.compiler || new JotCompiler(vfs);

  // Register standard operators if not already provided
  if (!options.compiler) {
    const boxSpec = {
      path: 'shape/box',
      schema: { arguments: { width: { type: 'number', default: 10 } } },
    };
    compiler.registerOperator('Box', boxSpec);
    compiler.registerOperator('box', boxSpec);

    const orbSpec = {
      path: 'shape/orb',
      schema: { arguments: { diameter: { type: 'number', default: 10 } } },
    };
    compiler.registerOperator('Orb', orbSpec);
    compiler.registerOperator('orb', orbSpec);

    compiler.registerOperator('Hexagon', {
      path: 'shape/hexagon/full',
      schema: { arguments: { diameter: { type: 'number' } } },
    });

    compiler.registerOperator('offset', {
      path: 'op/offset',
      schema: { arguments: { radius: { type: 'number', default: 1 } } },
    });

    compiler.registerOperator('outline', {
      path: 'op/outline',
      schema: {
        arguments: { $in: { type: 'shape' } },
        outputs: { $out: { type: 'shape' } },
      },
    });

    const rxSpec = {
      path: 'op/rotateX',
      schema: {
        arguments: { source: { type: 'shape' }, turns: { type: 'number' } },
      },
    };
    compiler.registerOperator('rx', rxSpec);

    compiler.registerOperator('pdf', {
      path: 'op/pdf',
      schema: {
        arguments: { $in: { type: 'shape' }, path: { type: 'string' } },
      },
      metadata: { aliases: { $out: '$in' } },
    });

    // Test operators for implicit context
    compiler.registerOperator('A', {
      path: 'op/A',
      schema: { arguments: { value: { type: 'number' } } },
    });
    compiler.registerOperator('B', {
      path: 'op/B',
      schema: { arguments: { $in: { type: 'shape' }, arg: { type: 'number' } } },
    });
    compiler.registerOperator('C', {
      path: 'op/C',
      schema: { arguments: { $in: { type: 'shape' }, arg: { type: 'number' } } },
    });
  }

  vfs.registerProvider('jot/eval', async (v, selector, context) => {
    const { expression, params = {} } = selector.parameters;
    if (!expression) return null;

    try {
      // 1. Parse the expression
      const ast = parser.parse(expression);

      // 2. Resolve AST to VFS Selectors using the compiler
      // (Compiler handles symbol resolution and schema mapping)
      const resolved = await compiler.evaluate(ast, params);

      // 3. Chain the request to the mesh
      if (Array.isArray(resolved)) {
        // If it's a sequence, we might need a group/sequence op
        return await v.read('op/group', { sources: resolved }, context);
      }
      return await v.read(resolved.path, resolved.parameters, context);
    } catch (err) {
      console.error(`[JotProvider] Evaluation error:`, err);
      return null;
    }
  });

  /**
   * Fallback provider for any .jot file request.
   * Maps vfs:/path/to/file.jot?p=v -> jot/eval({ expression: <content of file.jot>, params: { p: v } })
   */
  vfs.registerProvider('.jot', async (v, selector, context) => {
    if (!selector.path.endsWith('.jot')) return null;

    try {
      // 1. Read the raw .jot file content from the mesh
      // We strip parameters to get the base file
      const expression = await v.readText(selector.path, {}, context);
      if (!expression) return null;

      // 2. Delegate to jot/eval with the current parameters
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
