import { VFS } from './src/vfs_core.js';
import http from 'node:http';

async function test() {
  const vfs = new VFS();
  const resp = await fetch('http://localhost:9091/spy', {
    method: 'POST',
    body: JSON.stringify({ path: 'sys/schema', parameters: {} }),
  });

  if (!resp.ok) {
    console.error('Spy failed:', resp.status);
    return;
  }

  console.log('--- Spy Bundle Parse ---');
  for await (const chunk of VFS.parseVFSBundle(resp.body)) {
    console.log('Path:', chunk.selector.path);
    console.log('Params:', JSON.stringify(chunk.selector.parameters));
    const schema = JSON.parse(new TextDecoder().decode(chunk.data));
    console.log('Origin in Schema:', schema._origin);
    console.log('---');
  }
}

test();
