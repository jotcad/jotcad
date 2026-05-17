import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

const parser = new JotParser();
const compiler = new JotCompiler();

compiler.registerOperator('jot/rz', {
    path: 'jot/rz',
    schema: {
        arguments: [
            { name: '$in', type: 'jot:shape', affiliate: '$out' },
            { name: 'turns', type: 'jot:numbers' }
        ],
        outputs: { "$out": { type: "jot:shape" } }
    }
});

const testRz = async (str) => {
    try {
        console.log(`Input: "${str}"`);
        const ast = parser.parse(str);
        const results = await compiler.evaluate(ast, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        console.log(`Result: ${JSON.stringify(results[0].selector, null, 2)}`);
    } catch (e) {
        console.log(`Error: ${e.message}`);
    }
    console.log('---');
};

(async () => {
    // Mock shape A
    compiler.registerOperator('jot/A', { path: 'jot/A', schema: { arguments: [], outputs: { "$out": { type: "jot:shape" } } } });
    
    await testRz('A().rz(-1/6, 1/6)');
})();
