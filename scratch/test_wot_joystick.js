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
    console.log(`[WoT Client] Connecting to local Zenoh router at ${wsUrl}...`);
    const config = new ZenohConfig(wsUrl);
    const session = await zenohOpen(config);
    console.log("[WoT Client] Connected to Zenoh router.");

    // 1. Fetch the Thing Description dynamically from the ESP32 node over VFS Queryable
    const tdQueryExpr = 'jot/vfs/op/sensor/joystick/td';
    console.log(`[WoT Client] Querying device for its TD contract over VFS path: ${tdQueryExpr}...`);

    let td = null;
    const replies = await session.get(tdQueryExpr);
    for await (const reply of replies) {
        const result = reply.result();
        if (result instanceof Error || result.constructor.name === 'ReplyError') {
            continue;
        }
        
        const sample = result;
        const recordBytes = sample.payload().toBytes();
        const decoded = decodeRecord(recordBytes);
        if (decoded && decoded.header.status === 200) {
            const rawJson = new TextDecoder().decode(decoded.payload);
            td = JSON.parse(rawJson);
            break;
        }
    }

    if (!td) {
        console.error("\n[Error] Could not retrieve Thing Description from the device. Make sure the ESP32-S2 is connected, flashed, and online!");
        await session.close();
        process.exit(1);
    }

    console.log(`\n[WoT Client] SUCCESSFULLY fetched Thing Description dynamically!`);
    console.log(`  Device Title:  "${td.title}"`);
    console.log(`  Description:   "${td.description}"`);
    console.log(`  Device URN:    "${td.id}"`);

    // 2. Parse the properties & dynamic visualization annotations
    const coordProp = td.properties.coordinates;
    const form = coordProp.forms[0];
    const targetTopic = form['jot:topic'];   // "sensor/joystick"
    const visualizerType = coordProp['jot:visualizer']; // "joystick-grid-2d"

    console.log(`  Annotated Visualizer: "${visualizerType}"`);
    console.log(`  Data stream topic:    "${targetTopic}"`);
    console.log("\nStarting visualizer loop in 3 seconds...");
    await new Promise(resolve => setTimeout(resolve, 3000));

    // Clear console to prepare for visualizer loop
    process.stdout.write('\x1Bc');

    // 3. Subscribe using the details dynamically resolved from the TD
    await session.declareSubscriber(targetTopic, (sample) => {
        try {
            const data = JSON.parse(sample.value.toString());
            const x = data.x;
            const y = data.y;
            const pressed = data.pressed;

            if (visualizerType === 'joystick-grid-2d') {
                renderGrid(x, y, pressed);
            }
        } catch (err) {
            console.error("Error rendering frame:", err);
        }
    });

    // Keep running
    await new Promise(() => {});
}

// Custom semantic renderer for 'joystick-grid-2d'
function renderGrid(x, y, pressed) {
    const GRID_SIZE = 9;
    
    // Scale 12-bit ADC value (0-4095) to grid coordinate (0-8)
    const dotX = Math.min(Math.max(Math.floor((x / 4096) * GRID_SIZE), 0), GRID_SIZE - 1);
    const dotY = Math.min(Math.max(Math.floor((y / 4096) * GRID_SIZE), 0), GRID_SIZE - 1);

    // Clear terminal screen and move cursor to top-left
    process.stdout.write('\x1B[H\x1B[2J');
    
    console.log("======================================");
    console.log("    W3C Web of Things: Live View      ");
    console.log("======================================");
    console.log(`  Raw X: ${x.toString().padEnd(4)}  |  Raw Y: ${y.toString().padEnd(4)}`);
    console.log(`  Button: ${pressed ? "💥 PRESSED 💥" : "RELEASED"}`);
    console.log("--------------------------------------");

    // Draw the 9x9 visualizer grid
    for (let r = 0; r < GRID_SIZE; r++) {
        let line = "  | ";
        for (let c = 0; c < GRID_SIZE; c++) {
            if (r === dotY && c === dotX) {
                // Draw the joystick pointer (highlighted if button is pressed)
                line += pressed ? "🔴 " : "⚪ ";
            } else if (r === 4 && c === 4) {
                // Center origin indicator
                line += "┼ ";
            } else {
                // Background grid
                line += "· ";
            }
        }
        line += "|";
        console.log(line);
    }
    console.log("======================================");
    console.log(" Move joystick or click switch on GPIO 7");
}

main().catch(console.error);
