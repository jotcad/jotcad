import { spawn } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs/promises';

const CPP_OPS_PATH = path.resolve('geo/bin/ops');
const PORT_CPP = 9555;
const PEER_ID = 'geo-ops-node-test';
const STORAGE_CPP = path.resolve('.vfs_storage_reproduce_schema');

async function run() {
  console.log('--- REPRODUCING SCHEMA ORIGIN ISSUE ---');

  // Cleanup
  await fs.rm(STORAGE_CPP, { recursive: true, force: true }).catch(() => {});
  await fs.mkdir(STORAGE_CPP, { recursive: true });

  console.log(`Starting C++ node ${PEER_ID} on port ${PORT_CPP}...`);
  const cppNode = spawn(CPP_OPS_PATH, [], {
    env: {
      ...process.env,
      PORT: PORT_CPP.toString(),
      PEER_ID,
      STORAGE_DIR: STORAGE_CPP,
    },
  });

  cppNode.stdout.on('data', (data) => console.log(`[CPP STDOUT] ${data}`));
  cppNode.stderr.on('data', (data) => console.error(`[CPP STDERR] ${data}`));

  // Wait for node to start and register its schemas
  await new Promise((resolve) => setTimeout(resolve, 3000));

  try {
    console.log('Spying on sys/schema via HTTP...');
    const resp = await fetch(`http://localhost:${PORT_CPP}/spy`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path: 'sys/schema', parameters: {} }),
    });

    if (!resp.ok) {
      throw new Error(`Spy failed: ${resp.status} ${resp.statusText}`);
    }

    const buffer = await resp.arrayBuffer();
    const text = new TextDecoder().decode(buffer);

    console.log('--- RAW SPY OUTPUT ---');
    console.log(text);
    console.log('----------------------');

    if (text.includes(PEER_ID)) {
      console.log('SUCCESS: PEER_ID found in spy output.');
    } else {
      console.log('FAILURE: PEER_ID NOT found in spy output.');
    }
  } catch (err) {
    console.error('Error during reproduction:', err);
  } finally {
    console.log('Killing C++ node...');
    cppNode.kill();
    // Wait a bit for it to die
    await new Promise((resolve) => setTimeout(resolve, 500));
  }
}

run().catch(console.error);
