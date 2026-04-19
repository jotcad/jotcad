import { VFS } from './fs/src/index.js';

async function run() {
    const vfs = new VFS({ id: 'test-client' });
    const selector = {
        path: 'jot/offset',
        parameters: {
            $in: {
                path: 'jot/Triangle/equilateral',
                parameters: { diameter: 30 }
            },
            radius: 5
        }
    };
    
    console.log('Fetching Triangle Offset...');
    const response = await fetch('http://localhost:9092/read', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(selector)
    });
    
    if (response.ok) {
        const data = await response.json();
        const meshResponse = await fetch('http://localhost:9092/read', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data.geometry)
        });
        const meshText = await meshResponse.text();
        
        const zfs = `
=${JSON.stringify(data).length} files/main.json
${JSON.stringify(data)}
=${meshText.length} assets/text/${data.geometry.path}?${JSON.stringify(data.geometry.parameters)}
${meshText}
`.trim();
        console.log('--- ZFS START ---');
        console.log(zfs);
        console.log('--- ZFS END ---');
    } else {
        console.error('Failed to fetch:', response.status);
    }
}

run().catch(console.error);
