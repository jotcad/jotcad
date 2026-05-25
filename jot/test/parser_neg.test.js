import { JotParser } from '../src/parser.js';
import { log } from '../../fs/src/log.js';

const parser = new JotParser();
const test = (str) => {
    try {
        log(`Input: "${str}"`);
        const tokens = parser._tokenize(str);
        log(`Tokens: ${JSON.stringify(tokens)}`);
        const ast = parser.parse(str);
        log(`AST: ${JSON.stringify(ast, null, 2)}`);
    } catch (e) {
        log(`Error: ${e.message}`);
    }
    log('---');
};

test('1/5');
test('-1/6');
test('Box(-1/6)');
test('-myVar');
test('--5');
test('1/-2');
test('Box(1/-2)');
