import { JotParser } from './jot/src/parser.js';
import { JotCompiler } from './jot/src/compiler.js';

const parser = new JotParser();
const compiler = new JotCompiler();

// Mock an operator that takes an interval
compiler.registerOperator('jot/RangeTest', {
    path: 'jot/RangeTest',
    schema: {
        arguments: [
            { name: 'range', type: 'jot:interval' }
        ],
        outputs: { "$out": { type: "jot:shape" } }
    }
});

const test = async (str) => {
    try {
        console.log(`Input: "${str}"`);
        const ast = parser.parse(str);
        const results = await compiler.evaluate(ast);
        console.log(`Resulting Range: ${JSON.stringify(results[0].selector.parameters.range)}`);
    } catch (e) {
        console.log(`Error: ${e.message}`);
    }
    console.log('---');
};

(async () => {
    await test('RangeTest([4, -2])');
    await test('RangeTest([-2, 4])');
    await test('RangeTest([5])'); // Half-interval check
    await test('RangeTest([-5])'); // Negative half-interval
})();
