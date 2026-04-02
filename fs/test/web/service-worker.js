import { VFS, IndexedDBStorage } from '/src/vfs_browser.js';

const storage = new IndexedDBStorage();
const vfs = new VFS({ id: 'service-worker', storage });

self.addEventListener('install', (event) => {
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(self.clients.claim());
});

const browserPeer = {
  send: async (data) => {
    const clients = await self.clients.matchAll();
    clients.forEach((client) => client.postMessage(data));
  },
};
vfs.connect(browserPeer);

self.addEventListener('message', (event) => {
  vfs.receive(event.data, browserPeer);
});

vfs.events.on('state', (event) => {
  console.log(
    `[VFS service-worker] State Change: ${event.path} ${event.state} (from ${event.source})`
  );
});

async function run() {
  console.log('Service Worker VFS ready.');

  // Test Scenario:
  // SW watches for 'sw-done' demand
  // Then reads 'worker-done' and writes 'sw-done'
  const watcher = vfs.watch('sw-done', { states: ['PENDING'] });
  for await (const event of watcher) {
    console.log('SW triggered by sw-done demand!');
    const hasLease = await vfs.lease('sw-done', 5000);
    if (hasLease) {
      // Cascading demand: SW reads from Worker
      const workerStream = await vfs.read('worker-done');
      console.log('SW received worker-done stream!');

      const stream = new ReadableStream({
        start(controller) {
          controller.enqueue(new TextEncoder().encode('Hello from SW'));
          controller.close();
        },
      });
      await vfs.write('sw-done', {}, stream);
    }
  }
}

run();
