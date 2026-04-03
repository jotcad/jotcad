import { createSignal, createResource } from 'solid-js';

/**
 * A reactive VFS client for the UI.
 * Connects to the Node.js VFS Hub and provides Solid.js signals for state.
 */
export class UIBlackboard {
  constructor(baseUrl = '/vfs') {
    this.baseUrl = baseUrl;
    this.eventSource = null;
    
    // Reactive state: Map of CID -> State
    const [graph, setGraph] = createSignal({});
    this.graph = graph;
    this.setGraph = setGraph;
  }

  async start() {
    // 1. Initial Sync
    const resp = await fetch(`${this.baseUrl}/states`);
    if (resp.ok) {
      const states = await resp.json();
      const newGraph = {};
      for (const s of states) {
        newGraph[s.cid] = s;
      }
      this.setGraph(newGraph);
    }

    // 2. Listen for Live Updates (SSE)
    this.eventSource = new EventSource(`${this.baseUrl}/watch?peerId=ui-${Math.random().toString(36).slice(2)}`);
    this.eventSource.onmessage = (e) => {
      const event = JSON.parse(e.data);
      this.setGraph(prev => ({
        ...prev,
        [event.cid]: {
          ...prev[event.cid],
          ...event
        }
      }));
    };
  }

  async read(path, parameters = {}) {
    const resp = await fetch(`${this.baseUrl}/read`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path, parameters })
    });
    return resp.ok ? resp.body : null;
  }

  async tickle(path, parameters = {}) {
    await fetch(`${this.baseUrl}/tickle`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path, parameters })
    });
  }

  stop() {
    this.eventSource?.close();
  }
}

export const blackboard = new UIBlackboard(`http://${window.location.hostname}:9090/vfs`);
 black;
