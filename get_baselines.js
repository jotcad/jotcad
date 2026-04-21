import { getCID } from './fs/src/index.js';
const cases = [
    { name: 'empty', selector: { path: '', parameters: {} } },
    { name: 'integers', selector: { path: 'jot/Box', parameters: { width: 10, height: 10, depth: 0 } } },
    { name: 'floats', selector: { path: 'jot/Box', parameters: { width: 10.5, height: 10.5, depth: 0.1 } } },
    { name: 'nested', selector: { path: 'jot/cut', parameters: { $in: { path: 'jot/Box', parameters: {} } } } },
    { name: 'integers as floats', selector: { path: 'jot/Box', parameters: { width: 10.0, height: 10.0, depth: 0.0 } } },
];
async function run() {
    for (const c of cases) {
        console.log(`${c.name}: ${await getCID(c.selector)}`);
    }
}
run();
