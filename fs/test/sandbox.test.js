import { test } from 'node:test';
import assert from 'node:assert';
import { MemSandbox, DiskSandbox } from '../src/sandbox.js';
import { Readable } from 'node:stream';

test('Sandbox Marshalling', async (t) => {
  await t.test('MemSandbox: put and get', async () => {
    const sandbox = new MemSandbox();

    // Marshall a string
    await sandbox.put('/in/test.txt', 'hello sandbox');

    // Marshall a stream
    const stream = new ReadableStream({
      start(c) {
        c.enqueue(new TextEncoder().encode('streamed data'));
        c.close();
      },
    });
    await sandbox.put('/in/stream.txt', stream);

    // Verify files in memfs
    assert.strictEqual(
      sandbox.fs.readFileSync('/in/test.txt', 'utf8'),
      'hello sandbox'
    );
    assert.strictEqual(
      sandbox.fs.readFileSync('/in/stream.txt', 'utf8'),
      'streamed data'
    );

    // Unmarshall
    const outStream = await sandbox.get('/in/test.txt');
    let outData = '';
    const reader = outStream.getReader();
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      outData += new TextDecoder().decode(value);
    }
    assert.strictEqual(outData, 'hello sandbox');

    sandbox.close();
  });

  await t.test('DiskSandbox: put and get', async () => {
    const sandbox = new DiskSandbox();

    await sandbox.put('input.txt', 'disk data');

    // Check real disk
    const fs = await import('node:fs');
    const path = await import('node:path');
    assert.ok(fs.existsSync(path.join(sandbox.root, 'input.txt')));

    // Unmarshall as Node stream
    const outStream = await sandbox.get('input.txt');
    let outData = '';
    for await (const chunk of outStream) {
      outData += chunk.toString();
    }
    assert.strictEqual(outData, 'disk data');

    await sandbox.close();
    assert.ok(!fs.existsSync(sandbox.root));
  });
});
