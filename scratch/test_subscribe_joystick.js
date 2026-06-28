import { open as zenohOpen, Config as ZenohConfig } from '@eclipse-zenoh/zenoh-ts';

async function main() {
    console.log("[Joystick Monitor] Connecting to local Zenoh router at ws://127.0.0.1:10000...");
    const config = new ZenohConfig('ws://127.0.0.1:10000');
    const session = await zenohOpen(config);
    console.log("[Joystick Monitor] Connected! Listening on 'sensor/joystick'...\n");

    // Subscribe to sensor/joystick
    await session.declareSubscriber("jot/vfs/pub/**", {
        handler: (sample) => {
            const payloadBytes = sample.payload().toBytes();
            const payloadString = new TextDecoder().decode(payloadBytes);
            const key = sample.keyexpr().toString();
            console.log(`[Joystick Event] ${key} -> ${payloadString}`);
        }
    });

    // Keep running
    await new Promise(() => {});
}

main().catch(console.error);
