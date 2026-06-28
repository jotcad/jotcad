import { open as zenohOpen, Config as ZenohConfig } from '@eclipse-zenoh/zenoh-ts';

// Helper to decode custom JotCAD VFS binary envelope (4-byte header length + header + body)
function decodeRecord(recordBytes) {
    if (recordBytes.length < 4) return null;
    const view = new DataView(recordBytes.buffer, recordBytes.byteOffset, recordBytes.byteLength);
    const headerLen = view.getUint32(0, false);
    if (recordBytes.length < 4 + headerLen) return null;
    const headerBytes = recordBytes.subarray ? recordBytes.subarray(4, 4 + headerLen) : recordBytes.slice(4, 4 + headerLen);
    const headerStr = new TextDecoder().decode(headerBytes);
    const header = JSON.parse(headerStr);
    const payload = recordBytes.subarray ? recordBytes.subarray(4 + headerLen) : recordBytes.slice(4 + headerLen);
    return { header, payload };
}

async function main() {
    const wsUrl = 'ws://127.0.0.1:10000';
    console.log(`Connecting to Zenoh router at ${wsUrl}...`);
    const config = new ZenohConfig(wsUrl);
    const session = await zenohOpen(config);
    console.log("Connected.");

    const queryExpr = 'jot/vfs/op/sensor/joystick';
    console.log(`Querying: ${queryExpr}...`);

    const replies = await session.get(queryExpr);
    let found = false;
    for await (const reply of replies) {
        const result = reply.result();
        if (result instanceof Error || result.constructor.name === 'ReplyError') {
            console.error("Reply error:", result);
            continue;
        }
        
        const sample = result;
        const recordBytes = sample.payload().toBytes();
        const decoded = decodeRecord(recordBytes);
        if (decoded) {
            console.log("\nResponse Header:", JSON.stringify(decoded.header, null, 2));
            const rawJson = new TextDecoder().decode(decoded.payload);
            console.log("Response Body:", rawJson);
            found = true;
        }
    }

    if (!found) {
        console.log("No response received for query.");
    }

    await session.close();
}

main().catch(console.error);
