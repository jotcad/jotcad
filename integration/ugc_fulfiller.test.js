import { runIntegrationTest } from './harness.js';
import { Selector } from '../fs/src/index.js';

runIntegrationTest('UGC User-Defined Fulfiller & Expression Evaluation', async ({ compiler, parser, readData, vfs, mesh }) => {
  // 1. Define a user fulfiller for 'user/CustomBox'
  // When 'user/CustomBox' is invoked with { width, height }, it evaluates a Jot expression: Box(width, height, 10)
  const userOpPath = 'user/CustomBox';
  
  vfs.registerProvider(userOpPath, async (vfsNode, selector, context) => {
    const width = selector.parameters.width || 10;
    const height = selector.parameters.height || 10;
    
    console.log(`[User Fulfiller] Called for user/CustomBox with width=${width}, height=${height}`);

    // Evaluate inner Jot expression for the fulfiller
    const innerScript = `Box(${width}, ${height}, 10) -> $out`;
    const ast = parser.parse(innerScript);
    const results = await compiler.evaluate(ast, {}, { outputs: { $out: { type: 'jot:shape' } } });
    const outTerm = results.find(t => t.port === '$out');
    
    // Fulfillers MUST return { stream, metadata }
    return vfsNode.readSelector(outTerm.selector);
  }, ['width', 'height']);

  // Register the user operator schema with the compiler
  compiler.registerOperator('CustomBox', {
    path: userOpPath,
    schema: {
      path: userOpPath,
      arguments: [
        { name: 'width', type: 'jot:number', default: 10 },
        { name: 'height', type: 'jot:number', default: 10 }
      ],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  // 2. Evaluate a Jot expression that calls the user-defined fulfiller: CustomBox(20, 30)
  const script = `CustomBox(20, 30) -> $out`;
  console.log(`[Test] Evaluating Jot script calling user fulfiller: "${script}"`);

  const ast = parser.parse(script);
  const results = await compiler.evaluate(ast, {}, { outputs: { $out: { type: 'jot:shape' } } });
  
  const outTerm = results.find(t => t.port === '$out');
  if (!outTerm) {
    throw new Error('Test Error: No $out terminal produced by script evaluation');
  }

  // 3. Read and verify the fulfilled shape data using harness readData helper
  const shapeData = await readData(outTerm.selector);
  console.log(`[Test] Successfully fulfilled shape geometry:`, JSON.stringify(shapeData).slice(0, 100) + '...');
  
  if (!shapeData) {
    throw new Error('Test Error: Fulfilled shape data is empty (null)');
  }
});
