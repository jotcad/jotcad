import { JotParser } from './jot/src/parser.js';
import { JotCompiler } from './jot/src/compiler.js';

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

const test = async (str) => {
    try {
        console.log(`Input: "${str}"`);
        const ast = parser.parse(str);
        const res = await compiler.evaluate(ast);
        console.log(`Result: ${JSON.stringify(res[0], null, 2)}`);
    } catch (e) {
        console.log(`Error: ${e.message}`);
    }
    console.log('---');
};

(async () => {
    // Mock shape A
    compiler.registerOperator('jot/A', { path: 'jot/A', schema: { arguments: [], outputs: { "$out": { type: "jot:shape" } } } });
    
    await test('A().rz(-1/6, 1/6)');
})();
