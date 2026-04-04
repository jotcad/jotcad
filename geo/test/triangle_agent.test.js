import { test } from 'node:test';
import assert from 'node:assert';
import { VFS, registerVFSRoutes } from '../../fs/src/index.js';
import { Dispatcher } from '../src/dispatcher.js';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('C++ Triangle Agent (Normalization)', { timeout: 15000 }, async (t) => {
  const vfs = new VFS({ id: 'hub' });
  const server = http.createServer();
  registerVFSRoutes(vfs, server, '/vfs');

  const port = await new Promise((resolve) =>
    server.listen(0, '127.0.0.1', () => resolve(server.address().port))
  );
  const hubUrl = `http://127.0.0.1:${port}/vfs`;

  const dispatcher = new Dispatcher(vfs, {
    hubUrl,
    binDir: path.join(__dirname, '..', 'bin'),
  });
  dispatcher.register('shape/triangle', 'triangle_agent');
  dispatcher.start();

  await t.test('SAS and Equilateral requests result in same geometry', async () => {
    const sasStream = await vfs.read('shape/triangle', { form: 'SAS', a: 10, b: 10, angle: 60 });
    const sasShape = JSON.parse(new TextDecoder().decode(await streamToBuffer(sasStream)));
    
    const eqStream = await vfs.read('shape/triangle', { form: 'equilateral', side: 10 });
    const eqShape = JSON.parse(new TextDecoder().decode(await streamToBuffer(eqStream)));

    assert.strictEqual(sasShape.geometry, eqShape.geometry);
  });

  await t.test('vfs.snapshot captures the full triangle graph', async () => {
    const eqCoord = 'shape/triangle';
    const eqParams = { form: 'equilateral', side: 10 };
    const canonicalGeoParams = { a: 10, b: 10, c: 10, type: 'triangle' };

    const shapeSel = { path: eqCoord, parameters: eqParams };
    const linkSel = { path: eqCoord + '/geometry', parameters: eqParams };
    const geoSel = { path: 'geo/mesh', parameters: canonicalGeoParams };

    const cidS = await vfs.getCID(shapeSel);
    const cidL = await vfs.getCID(linkSel);
    const cidG = await vfs.getCID(geoSel);

    // Assert individual CIDs for visibility
    assert.strictEqual(cidS, 'c5ea85ccd8d19ed722c400d2a85784923bb06cfe8fb1d5c034326aed155e1724');
    assert.strictEqual(cidL, '0607dde43995dcf08b380c11dd72a13521141be1d56d03971b277290e88fb12c');
    assert.strictEqual(cidG, '888d65d7107037281e9b4dc4d5214ea569b076bdc5bcf562d93321118d1e8ec4');

    // 1. Snapshot the set of coordinates
    const zfsBuffer = await vfs.snapshot([shapeSel, linkSel, geoSel]);
    const actualText = zfsBuffer.toString();
    
    // 2. Assert the whole ZFS text verbatim
    const expectedText = `
=119 vfs/${cidS}.meta
{
  "path": "${shapeSel.path}",
  "parameters": {
    "form": "equilateral",
    "side": 10
  },
  "state": "AVAILABLE"
}
=165 vfs/${cidS}.data
{
  "geometry": "vfs:./geometry",
  "parameters": {
    "a": 10.0,
    "b": 10.0,
    "c": 10.0,
    "type": "triangle"
  },
  "tags": {
    "type": "triangle"
  }
}
=264 vfs/${cidL}.meta
{
  "path": "${linkSel.path}",
  "parameters": {
    "form": "equilateral",
    "side": 10
  },
  "state": "LINKED",
  "target": {
    "path": "geo/mesh",
    "parameters": {
      "a": 10,
      "b": 10,
      "c": 10,
      "type": "triangle"
    }
  }
}
=133 vfs/${cidG}.meta
{
  "path": "${geoSel.path}",
  "parameters": {
    "a": 10,
    "b": 10,
    "c": 10,
    "type": "triangle"
  },
  "state": "AVAILABLE"
}
=250 vfs/${cidG}.data
v 0.0000000000 0.0000000000 0.0000000000 0.0000000000 0.0000000000 0.0000000000
v 10.0000000000 0.0000000000 0.0000000000 10.0000000000 0.0000000000 0.0000000000
v 5.0000000000 8.6602540378 0.0000000000 5.0000000000 8.6602540378 0.0000000000
f 0 1 2
`;

    assert.strictEqual(actualText, expectedText);
  });

  // Cleanup
  dispatcher.stop();
  server.close();
  await vfs.close();
});

async function streamToBuffer(stream) {
  const chunks = [];
  for await (const chunk of stream) chunks.push(chunk);
  return Buffer.concat(chunks);
}
