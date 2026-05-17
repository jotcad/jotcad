import { JotParser } from '../src/parser.js';

const parser = new JotParser();
const test = (str) => {
    try {
        console.log(`Input: "${str}"`);
        const tokens = parser._tokenize(str);
        console.log(`Tokens: ${JSON.stringify(tokens)}`);
        const ast = parser.parse(str);
        console.log(`AST: ${JSON.stringify(ast, null, 2)}`);
    } catch (e) {
        console.log(`Error: ${e.message}`);
    }
    console.log('---');
};

test('1/5');
test('-1/6');
test('Box(-1/6)');
test('-myVar');
test('--5');
test('1/-2');
test('Box(1/-2)');
