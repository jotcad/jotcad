import { open as zenohOpen, Config as ZenohConfig } from '@eclipse-zenoh/zenoh-ts';

async function main() {
    console.log("[Joystick Monitor] Connecting to local Zenoh router at ws://127.0.0.1:10000...");
    const config = new ZenohConfig('ws://127.0.0.1:10000');
    const session = await zenohOpen(config);
    console.log("[Joystick Monitor] Connected! Listening on 'sensor/joystick'...\n");

    // Subscribe to sensor/joystick
    await session.declareSubscriber("sensor/joystick", (sample) => {
        const payloadString = sample.value.toString();
        console.log(`[Joystick Event] ${payloadString}`);
    });

    // Keep running
    await new Promise(() => {});
}

main().catch(console.error);
