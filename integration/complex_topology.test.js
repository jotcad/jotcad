import test from 'node:test';
import assert from 'node:assert';
import { VFS } from '../fs/src/vfs.js';
import { MeshLink } from '../fs/src/mesh_link.js';
import { Selector } from '../fs/src/cid.js';
import { launchSystem } from '../orchestrator.js';

test('Complex Hybrid VFS Mesh: 3-Node Topology Subscription & Notification', async (t) => {
    console.log('[Test Complex Topo] Launching linear cluster: C++ (1) <-> JS (Gateway) <-> C++ (2)...');
    
    // 1. Launch the complex test cluster profile
    const sys = await launchSystem('test/complex_topology');
    const JS_PORT = sys.ports.node_js;

    // Wait for JS Node to be healthy and fully listening
    console.log(`[Test Complex Topo] Waiting for JS Gateway to be ready on port ${JS_PORT}...`);
    let nodeReady = false;
    for (let i = 0; i < 50; i++) {
        try {
            const resp = await fetch(`http://localhost:${JS_PORT}/health`);
            if (resp.ok) { nodeReady = true; break; }
        } catch (e) {}
        await new Promise(r => setTimeout(r, 100));
    }
    assert.ok(nodeReady, 'JS Gateway Node failed to start listening in time.');

    // 2. Initialize the external subscriber node
    const nodeVFS = new VFS({ id: 'node-js-subscriber' });
    const mesh = new MeshLink(nodeVFS);
    await nodeVFS.init();

    try {
        // 3. Connect external subscriber -> JS Gateway node
        console.log(`[Test Complex Topo] Connecting subscriber node to JS Gateway on port ${JS_PORT}...`);
        await mesh.addPeer(`http://localhost:${JS_PORT}`);

        // 4. Set up reactive subscription listener for sys/topo
        const receivedTopo = new Map();
        nodeVFS.events.on('notify', (selector, payload) => {
            if (selector.path === 'sys/topo') {
                const peerId = payload.id || payload.peer;
                console.log(`[Test Complex Topo] Received sys/topo update from [${peerId}] with ${payload.neighbors?.length || 0} neighbors`);
                if (peerId && payload.neighbors) {
                    receivedTopo.set(peerId, payload.neighbors);
                }
            }
        });

        // 5. Subscribe to sys/topo mesh interest propagation
        console.log('[Test Complex Topo] Subscribing to sys/topo on JS Gateway...');
        await mesh.subscribe(Selector.fromObject({ path: 'sys/topo' }), Date.now() + 30000);

        // 6. Give the mesh network 3 seconds to propagate the subscription and stream back topology handshakes
        console.log('[Test Complex Topo] Waiting for multi-hop topology propagation...');
        await new Promise(r => setTimeout(r, 3000));

        // 7. Assertions: Verify the entire dynamic VFS mesh is structurally correct
        console.log('[Test Complex Topo] Verifying mesh graph structure...');

        // Gateway Node assertions
        assert.ok(receivedTopo.has('test_complex_topology_node_a'), 'Mesh should discover the JS Gateway ("test_complex_topology_node_a")');
        const gatewayNeighbors = receivedTopo.get('test_complex_topology_node_a');
        console.log('[Test Complex Topo] JS Gateway neighbors:', gatewayNeighbors);
        
        assert.ok(
            gatewayNeighbors.some(n => (n.id || n) === 'node-js-subscriber'),
            'JS Gateway must see the external subscriber node'
        );
        assert.ok(
            gatewayNeighbors.some(n => (n.id || n) === 'test_complex_topology_cpp_node_1'),
            'JS Gateway must see C++ Node 1'
        );
        assert.ok(
            gatewayNeighbors.some(n => (n.id || n) === 'test_complex_topology_cpp_node_2'),
            'JS Gateway must see C++ Node 2'
        );

        // C++ Node 1 assertions
        assert.ok(
            receivedTopo.has('test_complex_topology_cpp_node_1'),
            'Mesh should discover C++ Node 1 ("test_complex_topology_cpp_node_1")'
        );
        const cpp1Neighbors = receivedTopo.get('test_complex_topology_cpp_node_1');
        console.log('[Test Complex Topo] C++ Node 1 neighbors:', cpp1Neighbors);
        assert.ok(
            cpp1Neighbors.some(n => (n.id || n) === 'test_complex_topology_node_a'),
            'C++ Node 1 must see its peer link to the JS Gateway'
        );

        // C++ Node 2 assertions
        assert.ok(
            receivedTopo.has('test_complex_topology_cpp_node_2'),
            'Mesh should discover C++ Node 2 ("test_complex_topology_cpp_node_2")'
        );
        const cpp2Neighbors = receivedTopo.get('test_complex_topology_cpp_node_2');
        console.log('[Test Complex Topo] C++ Node 2 neighbors:', cpp2Neighbors);
        assert.ok(
            cpp2Neighbors.some(n => (n.id || n) === 'test_complex_topology_node_a'),
            'C++ Node 2 must see its peer link to the JS Gateway'
        );

        console.log('✔ All Complex Topology assertions verified successfully!');

    } finally {
        console.log('[Test Complex Topo] Tearing down cluster...');
        mesh.stop();
        await nodeVFS.close();
        await sys.stop();
    }
});
