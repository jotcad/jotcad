import WebSocket from 'ws';

const targetId = process.argv[2];
if (!targetId) {
  console.error("Usage: node discover_ota.js <target_node_id>");
  process.exit(1);
}

// Default VFS WebSocket URL (typically pointing to the local gateway)
const wsUrl = process.env.VFS_WS_URL || 'ws://localhost:9091/vfs-ws';

console.warn(`[OTA-DISCOVER] Connecting to VFS Gateway at ${wsUrl}...`);
const ws = new WebSocket(wsUrl);

// Timeout after 10 seconds if the target node is not responding or offline
const timeout = setTimeout(() => {
  console.error(`[OTA-DISCOVER] Error: Node ${targetId} is not online or failed to reply.`);
  ws.close();
  process.exit(2);
}, 10000);

ws.on('open', () => {
  // 1. Identify ourselves to get ACK
  ws.send(JSON.stringify({ 
    type: 'IDENTIFY', 
    peerId: `ota-discover-${Math.random().toString(36).slice(2, 6)}` 
  }));
});

let identified = false;

ws.on('message', (data) => {
  try {
    const msg = JSON.parse(data.toString());
    
    if (!identified) {
      if (msg.type === 'ACK') {
        identified = true;
        console.warn(`[OTA-DISCOVER] Connected. Subscribing to sys/ota...`);
        // 2. Subscribe to sys/ota (This subscription propagates and triggers the target microcontroller to reply)
        ws.send(JSON.stringify({
          type: 'SUB',
          selector: { path: 'sys/ota' },
          expiresAt: Date.now() + 60000
        }));
      }
      return;
    }

    // 3. Process the incoming reactive publication
    if (msg.type === 'PUB' && msg.selector?.path === 'sys/ota') {
      const payload = msg.payload;
      if (payload && payload.id === targetId && payload.ip) {
        clearTimeout(timeout);
        // Output ONLY the IP to stdout so parent shells can capture it
        console.log(payload.ip);
        ws.close();
        process.exit(0);
      }
    }
  } catch (err) {
    console.error(`[OTA-DISCOVER] Error processing message frame:`, err);
  }
});

ws.on('error', (err) => {
  console.error(`[OTA-DISCOVER] WebSocket connection error:`, err.message);
  clearTimeout(timeout);
  process.exit(3);
});
