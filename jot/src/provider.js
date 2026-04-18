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
      path: 'jot/offset',
      schema: { arguments: { radius: { type: 'jot:number', default: 1 } } },
    });

    compiler.registerOperator('outline', {
      path: 'jot/outline',
      schema: {
        arguments: { $in: { type: 'jot:shape' } },
        outputs: { $out: { type: 'jot:shape' } },
      },
    });

    compiler.registerOperator('points', {
      path: 'jot/points',
      schema: {
        arguments: { $in: { type: 'jot:shape' } },
        outputs: { $out: { type: 'jot:geometry' } },
      },
    });

    compiler.registerOperator('path', {
      path: 'jot/path',
      schema: {
        arguments: { $in: { type: 'jot:shape' } },
        outputs: { $out: { type: 'jot:geometry' } },
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

    compiler.registerOperator('Group', {
      path: 'jot/group',
      schema: {
        arguments: { shapes: { type: 'jot:shapes' } }
      }
    });

    compiler.registerOperator('group', {
      path: 'jot/group',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' },
          others: { type: 'jot:shapes' } 
        }
      }
    });

    compiler.registerOperator('and', {
      path: 'jot/group',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' },
          others: { type: 'jot:shapes' } 
        }
      }
    });

    compiler.registerOperator('cut', {
      path: 'jot/cut',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' },
          tools: { type: 'jot:shapes' } 
        }
      }
    });

    compiler.registerOperator('at', {
      path: 'jot/on',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' },
          targets: { type: 'jot:shapes' },
          op: { type: 'jot:operation' }
        }
      }
    });

    compiler.registerOperator('on', {
      path: 'jot/on',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' },
          targets: { type: 'jot:shapes' },
          op: { type: 'jot:operation' }
        }
      }
    });

    compiler.registerOperator('corners', {
      path: 'jot/corners',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' },
          proxy: { type: 'jot:boolean', default: true }
        }
      }
    });

    compiler.registerOperator('color', {
      path: 'jot/color',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' },
          color: { type: 'jot:string' }
        }
      }
    });

    const rxSpec = {
      path: 'jot/rotateX',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' }, 
          turns: { type: 'jot:numbers' } 
        },
      },
    };
    compiler.registerOperator('rx', rxSpec);
    compiler.registerOperator('rotateX', rxSpec);

    compiler.registerOperator('ry', {
      path: 'jot/rotateY',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' }, 
          turns: { type: 'jot:numbers' } 
        },
      },
    });
    compiler.registerOperator('rotateY', {
        path: 'jot/rotateY',
        schema: {
          arguments: { 
            $in: { type: 'jot:shape' }, 
            turns: { type: 'jot:numbers' } 
          },
        },
    });

    compiler.registerOperator('rz', {
      path: 'jot/rotateZ',
      schema: {
        arguments: { 
          $in: { type: 'jot:shape' }, 
          turns: { type: 'jot:numbers' } 
        },
      },
    });
    compiler.registerOperator('rotateZ', {
        path: 'jot/rotateZ',
        schema: {
          arguments: { 
            $in: { type: 'jot:shape' }, 
            turns: { type: 'jot:numbers' } 
          },
        },
    });

    compiler.registerOperator('pdf', {
      path: 'jot/pdf',
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
