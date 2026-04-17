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
      path: 'jot/Box',
      schema: { 
        arguments: { 
          width: { type: 'jot:number', default: 10 },
          height: { type: 'jot:number', default: 10 },
          depth: { type: 'jot:number', default: 0 }
        } 
      },
    };
    compiler.registerOperator('Box', boxSpec);
    compiler.registerOperator('box', boxSpec);

    compiler.registerOperator('Triangle', {
      path: 'jot/Triangle/equilateral',
      schema: { arguments: { diameter: { type: 'jot:number', default: 10 } } },
    });

    compiler.registerOperator('Hexagon', {
      path: 'jot/Hexagon/full',
      schema: { arguments: { diameter: { type: 'jot:number', default: 10 } } },
    });

    compiler.registerOperator('offset', {
      path: 'op/offset',
      schema: { arguments: { radius: { type: 'jot:number', default: 1 } } },
    });

    compiler.registerOperator('outline', {
      path: 'op/outline',
      schema: {
        arguments: { $in: { type: 'jot:shape' } },
        outputs: { $out: { type: 'jot:shape' } },
      },
    });

    compiler.registerOperator('nth', {
      path: 'jot/nth',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' }, 
          indices: { type: 'jot:numbers' } 
        },
      },
    });

    // and/Group is a Greedy consumer of shapes
    compiler.registerOperator('and', {
      path: 'op/group',
      schema: {
        arguments: { $in: { type: 'jot:shapes' } }
      }
    });

    const rxSpec = {
      path: 'op/rotateX',
      schema: {
        arguments: { 
          source: { type: 'jot:shape' }, 
          turns: { type: 'jot:numbers' } 
        },
      },
    };
    compiler.registerOperator('rx', rxSpec);
    compiler.registerOperator('rotateX', rxSpec);

    compiler.registerOperator('pdf', {
      path: 'op/pdf',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' }, 
          path: { type: 'jot:string' } 
        },
      },
      metadata: { aliases: { $out: '$in' } },
    });
  }

  vfs.registerProvider('jot/eval', async (v, selector, context) => {
    const { expression, params = {} } = selector.parameters;
    if (!expression) return null;

    try {
      // 1. Parse the expression
      const ast = parser.parse(expression);

      // 2. Resolve AST to VFS Selectors using the compiler
      const resolved = await compiler.evaluate(ast, params);

      // 3. Chain the request to the mesh
      // Note: If resolved is an array, it's a legacy sequence that should 
      // ideally be wrapped or handled by the caller. 
      // For now, we return it so the VFS can attempt to resolve the first item.
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
