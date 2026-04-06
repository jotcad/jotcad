import { VFS, RESTBridge } from '../../fs/src/index.js';
import { Dispatcher } from './dispatcher.js';

console.log('[Dispatcher] Initializing...');

const vfs = new VFS({ id: 'dispatcher' });
const bridge = new RESTBridge(vfs, 'http://localhost:9090/vfs');

const d = new Dispatcher(vfs, { 
    hubUrl: 'http://localhost:9090/vfs', 
    binDir: './geo/bin' 
});

d.register('shape/box', 'box_agent');
d.register('shape/triangle', 'triangle_agent');
d.register('shape/hexagon', 'hexagon_agent');

const hexagonUxCode = `
class HexagonEditor extends HTMLElement {
    constructor() {
        super();
        this.attachShadow({ mode: 'open' });
    }
    connectedCallback() { this.render(); }
    static get observedAttributes() { return ['radius', 'kerf', 'variant']; }
    attributeChangedCallback() { this.render(); }
    render() {
        const r = this.getAttribute('radius') || 10;
        const k = this.getAttribute('kerf') || 0;
        const v = this.getAttribute('variant') || 'full';
        this.shadowRoot.innerHTML = \`
            <style>
                .container { display: flex; flex-direction: column; gap: 8px; padding: 12px; background: rgba(255,255,255,0.05); border-radius: 8px; }
                label { display: flex; justify-content: space-between; font-size: 11px; font-family: monospace; color: #cbd5e1; align-items: center; }
                input[type="range"], select { flex: 1; margin-left: 8px; cursor: pointer; accent-color: #f97316; background: #1e293b; color: white; border: 1px solid #334155; border-radius: 4px; font-size: 10px; }
                span { width: 32px; text-align: right; font-weight: bold; color: white; }
                .header { font-size: 10px; font-weight: bold; text-transform: uppercase; letter-spacing: 2px; color: #f97316; margin-bottom: 4px; }
            </style>
            <div class="container">
                <div class="header">Hexagon Agent UI</div>
                <label>Radius <span>\${r}</span> <input type="range" id="r" min="1" max="100" value="\${r}" /></label>
                <label>Kerf <span>\${k}</span> <input type="range" id="k" min="-5" max="5" step="0.1" value="\${k}" /></label>
                <label>Variant 
                    <select id="v">
                        <option value="full" \${v === 'full' ? 'selected' : ''}>Full</option>
                        <option value="half" \${v === 'half' ? 'selected' : ''}>Half</option>
                        <option value="middle" \${v === 'middle' ? 'selected' : ''}>Middle (Rect)</option>
                        <option value="cap" \${v === 'cap' ? 'selected' : ''}>Cap (Triangle)</option>
                        <option value="sector" \${v === 'sector' ? 'selected' : ''}>Sector (1/6)</option>
                    </select>
                </label>
            </div>
        \`;
        ['r', 'k', 'v'].forEach(id => {
            this.shadowRoot.getElementById(id).addEventListener('input', (e) => {
                this.dispatchEvent(new CustomEvent('param-change', {
                    detail: {
                        radius: Number(this.shadowRoot.getElementById('r').value),
                        kerf: Number(this.shadowRoot.getElementById('k').value),
                        variant: this.shadowRoot.getElementById('v').value
                    },
                    bubbles: true, composed: true
                }));
            });
        });
    }
}
customElements.define('jotcad-hexagon-ux', HexagonEditor);
`;

const boxUxCode = `
class BoxEditor extends HTMLElement {
    constructor() {
        super();
        this.attachShadow({ mode: 'open' });
    }
    connectedCallback() { this.render(); }
    static get observedAttributes() { return ['width', 'height', 'depth']; }
    attributeChangedCallback() { this.render(); }
    render() {
        const w = this.getAttribute('width') || 10;
        const h = this.getAttribute('height') || 10;
        const d = this.getAttribute('depth') || 10;
        this.shadowRoot.innerHTML = \`
            <style>
                .container { display: flex; flex-direction: column; gap: 8px; padding: 12px; background: rgba(255,255,255,0.05); border-radius: 8px; }
                label { display: flex; justify-content: space-between; font-size: 11px; font-family: monospace; color: #cbd5e1; align-items: center; }
                input { flex: 1; margin-left: 8px; cursor: pointer; accent-color: #f97316; }
                span { width: 24px; text-align: right; font-weight: bold; color: white; }
                .header { font-size: 10px; font-weight: bold; text-transform: uppercase; letter-spacing: 2px; color: #f97316; margin-bottom: 4px; }
            </style>
            <div class="container">
                <div class="header">Box Agent UI</div>
                <label>W <span>\${w}</span> <input type="range" id="w" min="1" max="100" value="\${w}" /></label>
                <label>H <span>\${h}</span> <input type="range" id="h" min="1" max="100" value="\${h}" /></label>
                <label>D <span>\${d}</span> <input type="range" id="d" min="1" max="100" value="\${d}" /></label>
            </div>
        \`;
        ['w', 'h', 'd'].forEach(id => {
            this.shadowRoot.getElementById(id).addEventListener('input', (e) => {
                this.dispatchEvent(new CustomEvent('param-change', {
                    detail: {
                        width: Number(this.shadowRoot.getElementById('w').value),
                        height: Number(this.shadowRoot.getElementById('h').value),
                        depth: Number(this.shadowRoot.getElementById('d').value)
                    },
                    bubbles: true, composed: true
                }));
            });
        });
    }
}
customElements.define('jotcad-box-ux', BoxEditor);
`;

const pianoUxCode = `
class PianoEditor extends HTMLElement {
    constructor() {
        super();
        this.attachShadow({ mode: 'open' });
    }
    connectedCallback() { this.render(); }
    static get observedAttributes() { return ['note']; }
    attributeChangedCallback() { this.render(); }
    render() {
        const currentNote = this.getAttribute('note') || 'C4';
        const notes = ['C4', 'C#4', 'D4', 'D#4', 'E4', 'F4', 'F#4', 'G4', 'G#4', 'A4', 'A#4', 'B4', 'C5'];
        this.shadowRoot.innerHTML = \`
            <style>
                .keyboard { display: flex; background: #1e293b; padding: 10px; border-radius: 8px; gap: 2px; height: 80px; }
                .key { flex: 1; background: white; border-radius: 0 0 4px 4px; cursor: pointer; display: flex; align-items: flex-end; justify-content: center; font-size: 8px; font-family: monospace; padding-bottom: 5px; color: #64748b; transition: all 0.1s; }
                .key.black { background: #0f172a; color: white; height: 60%; margin-left: -10px; margin-right: -10px; z-index: 2; }
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
        \`;
        this.shadowRoot.querySelectorAll('.key').forEach(key => {
            key.addEventListener('click', () => {
                this.dispatchEvent(new CustomEvent('param-change', {
                    detail: { note: key.dataset.note },
                    bubbles: true, composed: true
                }));
            });
        });
    }
}
customElements.define('jotcad-piano-ux', PianoEditor);
`;

d.declareSchema('shape/box', {
    type: 'object',
    ux: 'vfs:/ui/components/box',
    componentName: 'jotcad-box-ux',
    properties: { width: { type: 'number', default: 10 }, height: { type: 'number', default: 10 }, depth: { type: 'number', default: 10 } }
});

d.declareSchema('instrument/piano', {
    type: 'object',
    ux: 'vfs:/ui/components/piano',
    componentName: 'jotcad-piano-ux',
    properties: { note: { type: 'string', default: 'C4' } }
});

d.declareSchema('shape/hexagon', {
    type: 'object',
    ux: 'vfs:/ui/components/hexagon',
    componentName: 'jotcad-hexagon-ux',
    properties: { 
        radius: { type: 'number', default: 10 }, 
        kerf: { type: 'number', default: 0 }, 
        variant: { type: 'string', enum: ['full', 'half', 'middle', 'cap'], default: 'full' } 
    }
});

d.declareSchema('shape/triangle', {
    type: 'object',
    properties: {
        form: { type: 'string', enum: ['SSS', 'SAS', 'equilateral'], default: 'equilateral' },
        side: { type: 'number', default: 10 }
    }
});

d.declareSchema('geo/mesh', { type: 'mesh', format: 'obj' });

console.log('[Dispatcher] Starting bridge and watch loop...');
await bridge.start();
await vfs.writeData('ui/components/box', {}, boxUxCode);
await vfs.writeData('ui/components/piano', {}, pianoUxCode);
await vfs.writeData('ui/components/hexagon', {}, hexagonUxCode);
await d.start();
