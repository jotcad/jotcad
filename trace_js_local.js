import { VFS, MemoryStorage } from './fs/src/vfs_core.js';

async function main() {
    const vfs = new VFS({ id: 'trace-vfs', storage: new MemoryStorage() });
    vfs.events.on('trace', ev => console.log(`[TRACE] ${ev.type}:`, ev.selector));

    const selector = { path: 'test/data', parameters: { id: 1 } };
    const data = { msg: 'hello' };

    console.log('--- Writing Data ---');
    await vfs.writeData(selector, data);

    console.log('--- Reading Data ---');
    const result = await vfs.readData(selector);
    console.log('RESULT:', JSON.stringify(result, null, 2));

    await vfs.close();
}
main();
