import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { SyncVFSAtomics, SyncVFSServer } from '../src/sync_vfs_atomics.js';
import { Worker, isMainThread, workerData, parentPort } from 'node:worker_threads';
import { fileURLToPath } from 'url';
import { Readable } from 'node:stream';

if (isMainThread) {
  test('SyncVFS Atomics Bridge (Node Worker Threads)', async (t) => {
    const vfs = new VFS({ id: 'node-main' });
    const sab = new SharedArrayBuffer(1024 * 1024);
    const server = new SyncVFSServer(vfs, sab);
    server.start(1); // Poll every 1ms

    const worker = new Worker(fileURLToPath(import.meta.url), {
      workerData: { sab }
    });

    await t.test('worker can perform synchronous reads and writes', async () => {
      // Pre-populate main VFS with a file for the worker to read
      await vfs.write('main-file', {}, Readable.from([Buffer.from('hello from main')]));

      // 1. Wait for worker to write something and read back
      const result = await new Promise((resolve, reject) => {
        worker.on('message', (msg) => {
          if (msg.type === 'DONE') resolve(msg);
          if (msg.type === 'ERROR') reject(new Error(msg.error));
        });
      });

      assert.strictEqual(result.readContent, 'hello from main');
      
      // 2. Check if main VFS has the data written by worker
      const stream = await vfs.read('worker-file');
      let data = '';
      for await (const chunk of stream) {
        data += new TextDecoder().decode(chunk);
      }
      assert.strictEqual(data, 'data from worker');
    });

    // Cleanup
    server.stop();
    await worker.terminate();
    await vfs.close();
  });
} else {
  // Worker Thread Logic
  const { sab } = workerData;
  const syncVFS = new SyncVFSAtomics(sab);

  async function run() {
    try {
      // 1. Synchronous Write
      syncVFS.write('worker-file', {}, new TextEncoder().encode('data from worker'));

      // 2. Synchronous Read (Wait for main thread to provide it)
      // Since we can't easily trigger the main thread from here except via the VFS demand,
      // let's assume the main thread is watching or we tickle it via a message.
      parentPort.postMessage({ type: 'READY' });
      
      // In a real scenario, this would block until another agent fulfills the demand.
      // For the test, we'll manually write to main VFS from the test runner.
      // Wait, the main test runner will write 'main-file' after receiving 'READY'.
      
      // Actually, let's keep it simple: 
      // Main thread writes 'main-file' BEFORE the worker reads it.
      // But how does worker know when? It doesn't care, it BLOCKS.
      const bytes = syncVFS.read('main-file', {});
      const content = new TextDecoder().decode(bytes);

      parentPort.postMessage({ type: 'DONE', readContent: content });
    } catch (err) {
      parentPort.postMessage({ type: 'ERROR', error: err.message });
    }
  }

  // We need to wait a bit for the main thread to set up
  setTimeout(run, 100);
}
