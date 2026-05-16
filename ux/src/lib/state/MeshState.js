import { createSignal } from 'solid-js';
import { createStore } from 'solid-js/store';

export const [graph, setGraph] = createSignal({});
export const [schemas, setSchemas] = createSignal({});
export const [pulse, setPulse] = createSignal([]);
export const [meshTopology, setMeshTopology] = createStore({ peers: [] });
export const [meshPositions, setMeshPositions] = createSignal({});

const [dynamicOpsRaw, setDynamicOpsRaw] = createSignal({});
export const dynamicOps = () => {
    const val = dynamicOpsRaw();
    return val;
};
export const setDynamicOps = (val) => {
    if (typeof val === 'function') {
        setDynamicOpsRaw(prev => val(prev));
    } else {
        setDynamicOpsRaw(val);
    }
};

export const [isConnected, setIsConnected] = createSignal(false);
export const [discoveryStatus, setDiscoveryStatus] = createSignal('idle');
