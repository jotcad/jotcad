import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const components = [
  {
    name: 'VFS Hub',
    command: 'node',
    args: ['fs/serve.js'],
    cwd: __dirname,
    env: { ...process.env, PORT: '9090' }
  },
  {
    name: 'C++ Dispatcher',
    command: 'node',
    args: [
      '--input-type=module',
      '-e',
      `
import { VFS, RESTBridge } from './fs/src/index.js';
import { Dispatcher } from './geo/src/dispatcher.js';

console.log('[Dispatcher] Initializing...');

process.on('unhandledRejection', (reason, promise) => {
    console.error('[Dispatcher] Unhandled Rejection at:', promise, 'reason:', reason);
    process.exit(1);
});

process.on('uncaughtException', (err) => {
    console.error('[Dispatcher] Uncaught Exception:', err);
    process.exit(1);
});

const vfs = new VFS({ id: 'dispatcher' });
const bridge = new RESTBridge(vfs, 'http://localhost:9090/vfs');

const d = new Dispatcher(vfs, { 
    hubUrl: 'http://localhost:9090/vfs', 
    binDir: './geo/bin' 
});

d.register('shape/box', 'box_agent');
d.register('shape/triangle', 'triangle_agent');

const boxUxCode = \`
class BoxEditor extends HTMLElement {
    constructor() {
        super();
        this.attachShadow({ mode: 'open' });
    }

    connectedCallback() {
        this.render();
    }

    static get observedAttributes() {
        return ['width', 'height', 'depth'];
    }

    attributeChangedCallback() {
        this.render();
    }

    render() {
        const w = this.getAttribute('width') || 10;
        const h = this.getAttribute('height') || 10;
        const d = this.getAttribute('depth') || 10;

        this.shadowRoot.innerHTML = \\\`
            <style>
                .container { display: flex; flex-direction: column; gap: 8px; padding: 12px; background: rgba(255,255,255,0.05); border-radius: 8px; }
                label { display: flex; justify-content: space-between; font-size: 11px; font-family: monospace; color: #cbd5e1; align-items: center; }
                input { flex: 1; margin-left: 8px; cursor: pointer; accent-color: #f97316; }
                span { width: 24px; text-align: right; font-weight: bold; color: white; }
                .header { font-size: 10px; font-weight: bold; text-transform: uppercase; letter-spacing: 2px; color: #f97316; margin-bottom: 4px; }
            </style>
            <div class="container">
                <div class="header">Box Agent UI</div>
                <label>W <span>\\\${w}</span> <input type="range" id="w" min="1" max="100" value="\\\${w}" /></label>
                <label>H <span>\\\${h}</span> <input type="range" id="h" min="1" max="100" value="\\\${h}" /></label>
                <label>D <span>\\\${d}</span> <input type="range" id="d" min="1" max="100" value="\\\${d}" /></label>
            </div>
        \\\`;

        ['w', 'h', 'd'].forEach(id => {
            this.shadowRoot.getElementById(id).addEventListener('input', (e) => {
                this.dispatchEvent(new CustomEvent('param-change', {
                    detail: {
                        width: Number(this.shadowRoot.getElementById('w').value),
                        height: Number(this.shadowRoot.getElementById('h').value),
                        depth: Number(this.shadowRoot.getElementById('d').value)
                    },
                    bubbles: true,
                    composed: true
                }));
            });
        });
    }
}
customElements.define('jotcad-box-ux', BoxEditor);
\`;

const pianoUxCode = \`
class PianoEditor extends HTMLElement {
    constructor() {
        super();
        this.attachShadow({ mode: 'open' });
    }

    connectedCallback() {
        this.render();
    }

    static get observedAttributes() {
        return ['note'];
    }

    attributeChangedCallback() {
        this.render();
    }

    render() {
        const currentNote = this.getAttribute('note') || 'C4';
        const notes = ['C4', 'C#4', 'D4', 'D#4', 'E4', 'F4', 'F#4', 'G4', 'G#4', 'A4', 'A#4', 'B4', 'C5'];

        this.shadowRoot.innerHTML = \\\`
            <style>
                .keyboard { display: flex; background: #1e293b; padding: 10px; border-radius: 8px; gap: 2px; height: 80px; }
                .key { 
                    flex: 1; 
                    background: white; 
                    border-radius: 0 0 4px 4px; 
                    cursor: pointer; 
                    display: flex; 
                    align-items: flex-end; 
                    justify-content: center; 
                    font-size: 8px; 
                    font-family: monospace; 
                    padding-bottom: 5px;
                    color: #64748b;
                    transition: all 0.1s;
                }
                .key.black { 
                    background: #0f172a; 
                    color: white; 
                    height: 60%; 
                    margin-left: -10px; 
                    margin-right: -10px; 
                    z-index: 2; 
                }
                .key.active { background: #f97316; color: white; transform: translateY(2px); }
                .key:hover { filter: brightness(0.9); }
                .header { font-size: 10px; font-weight: bold; color: #f97316; margin-bottom: 8px; text-transform: uppercase; letter-spacing: 1px; }
            </style>
            <div class="header">Virtual Piano Agent</div>
            <div class="keyboard">
                \${notes.map(note => {
                    const isBlack = note.includes('#');
                    const isActive = note === currentNote;
                    return \\\`<div class="key \${isBlack ? 'black' : ''} \${isActive ? 'active' : ''}" data-note="\${note}">\${note}</div>\\\`;
                }).join('')}
            </div>
        \\\`;

        this.shadowRoot.querySelectorAll('.key').forEach(key => {
            key.addEventListener('click', () => {
                this.dispatchEvent(new CustomEvent('param-change', {
                    detail: { note: key.dataset.note },
                    bubbles: true,
                    composed: true
                }));
            });
        });
    }
}
customElements.define('jotcad-piano-ux', PianoEditor);
\`;

d.declareSchema('shape/box', {
    type: 'object',
    ux: 'vfs:/ui/components/box',
    componentName: 'jotcad-box-ux',
    properties: {
        width: { type: 'number', default: 10 },
        height: { type: 'number', default: 10 },
        depth: { type: 'number', default: 10 }
    }
});

d.declareSchema('instrument/piano', {
    type: 'object',
    ux: 'vfs:/ui/components/piano',
    componentName: 'jotcad-piano-ux',
    properties: {
        note: { type: 'string', default: 'C4' }
    }
});

d.declareSchema('shape/triangle', {

    type: 'object',
    properties: {
        form: { type: 'string', enum: ['SSS', 'SAS', 'equilateral'], default: 'equilateral' },
        side: { type: 'number', default: 10 },
        a: { type: 'number' },
        b: { type: 'number' },
        c: { type: 'number' },
        angle: { type: 'number' }
    }
});

d.declareSchema('geo/mesh', {
    type: 'mesh',
    format: 'obj'
});

console.log('[Dispatcher] Starting bridge and watch loop...');
await bridge.start();
await vfs.writeData('ui/components/box', {}, boxUxCode);
await vfs.writeData('ui/components/piano', {}, pianoUxCode);
await d.start();
console.log('[Dispatcher] Watch loop exited?');
      `
    ],
    cwd: __dirname
  },
  {
    name: 'Interactive UX',
    command: 'npm',
    args: ['run', 'dev', '--', '--port', '3030'],
    cwd: path.join(__dirname, 'ux'),
    env: { ...process.env }
  }
];

const processes = new Map();
let shuttingDown = false;

function shutdown(reason) {
  if (shuttingDown) return;
  shuttingDown = true;
  
  if (reason) {
    console.error(`\n[Orchestrator] FATAL FAILURE: ${reason}`);
  }
  
  console.log('\n[Orchestrator] Shutting down all components...');
  for (const [name, child] of processes) {
    console.log(`[Orchestrator] Killing ${name}...`);
    child.kill();
  }
  
  // Exit with error if a failure caused the shutdown
  process.exit(reason ? 1 : 0);
}

function launch(component) {
  console.log(`[Orchestrator] Launching ${component.name}...`);
  
  const child = spawn(component.command, component.args, {
    cwd: component.cwd,
    env: component.env || process.env,
    stdio: 'pipe'
  });

  child.stdout.on('data', (data) => {
    process.stdout.write(`[${component.name}] ${data}`);
  });

  child.stderr.on('data', (data) => {
    process.stderr.write(`[${component.name} ERROR] ${data}`);
  });

  child.on('error', (err) => {
    shutdown(`${component.name} failed to start: ${err.message}`);
  });

  child.on('close', (code) => {
    if (!shuttingDown) {
      if (code !== 0) {
        shutdown(`${component.name} exited unexpectedly with code ${code}`);
      } else {
        shutdown(`${component.name} exited prematurely`);
      }
    }
    processes.delete(component.name);
  });

  processes.set(component.name, child);
}

process.on('SIGINT', () => shutdown());
process.on('SIGTERM', () => shutdown());

// Initial Launch
console.log('[Orchestrator] Starting JotCAD System...');
components.forEach(launch);

// Periodic check to ensure all processes are still in the Map and active
setInterval(() => {
    if (shuttingDown) return;
    for (const [name, child] of processes) {
        if (child.exitCode !== null) {
            shutdown(`${name} died with exit code ${child.exitCode}`);
        }
    }
}, 1000);
