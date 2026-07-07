/**
 * JotCAD Flow Visualization Library
 * A reusable ES6 library for grid visualization, isometric projections,
 * contour mapping, flow streamline tracing, and dashboard controls.
 */

/**
 * Converts RGB components to a standard hexadecimal string.
 * @param {number} r Red component (0-255)
 * @param {number} g Green component (0-255)
 * @param {number} b Blue component (0-255)
 * @returns {string} 6-character hex color starting with "#"
 */
export function rgbToHex(r, g, b) {
    const format = (c) => Math.max(0, Math.min(255, Math.round(c))).toString(16).padStart(2, '0');
    return `#${format(r)}${format(g)}${format(b)}`;
}

/**
 * Projects 3D grid space coordinates into 2D screen coordinates using isometric projection.
 * @param {number} x Grid X coordinate
 * @param {number} y Grid Y coordinate
 * @param {number} z Grid Z coordinate (elevation)
 * @param {Object} options Configuration parameters for the projection
 * @param {number} [options.spacing=5.2] Isometric node spacing width
 * @param {number} [options.heightScale=38.0] Vertical scaling factor for elevation
 * @param {number} [options.originX=500.0] Screen X origin offset
 * @param {number} [options.originY=250.0] Screen Y origin offset
 * @returns {Object} {x, y} coordinate in pixels
 */
export function projectIso(x, y, z, options = {}) {
    const spacing = options.spacing !== undefined ? options.spacing : 5.2;
    const heightScale = options.heightScale !== undefined ? options.heightScale : 38.0;
    const originX = options.originX !== undefined ? options.originX : 500.0;
    const originY = options.originY !== undefined ? options.originY : 250.0;

    const px = originX + (x - y) * 0.866 * spacing;
    const py = originY + (x + y) * 0.5 * spacing - z * heightScale;
    return { x: px, y: py };
}

/**
 * Interpolates cell-centered grid values (bedrock, surface water depth) to cell vertices (corners).
 * Necessary for drawing continuous 3D quad nets without stair-stepping visual artifacts.
 * @param {Array<Array<number>>} H_soil 2D bedrock elevation array
 * @param {Array<Array<number>>} h_surf 2D surface water depth array
 * @param {number} size Size of the grid
 * @returns {Object} {V_soil, V_water} where V_soil and V_water are (size+1)x(size+1) arrays
 */
export function interpolateVertexData(H_soil, h_surf, size) {
    const V_soil = Array.from({length: size + 1}, () => new Float32Array(size + 1));
    const V_water = Array.from({length: size + 1}, () => new Float32Array(size + 1));

    for (let y = 0; y <= size; y++) {
        for (let x = 0; x <= size; x++) {
            let count = 0;
            let sumSoil = 0;
            let sumWater = 0;
            for (let dy = -1; dy <= 0; dy++) {
                for (let dx = -1; dx <= 0; dx++) {
                    const cy = y + dy;
                    const cx = x + dx;
                    if (cy >= 0 && cy < size && cx >= 0 && cx < size) {
                        sumSoil += H_soil[cy][cx];
                        sumWater += h_surf[cy][cx];
                        count++;
                    }
                }
            }
            V_soil[y][x] = sumSoil / (count || 1);
            V_water[y][x] = sumWater / (count || 1);
        }
    }
    return { V_soil, V_water };
}

/**
 * Computes contour crossing segments for a single quad cell (Marching Squares).
 * @param {number} zA Elevation at corner A (top-left)
 * @param {number} zB Elevation at corner B (top-right)
 * @param {number} zC Elevation at corner C (bottom-right)
 * @param {number} zD Elevation at corner D (bottom-left)
 * @param {Object} pA Screen coordinate of A
 * @param {Object} pB Screen coordinate of B
 * @param {Object} pC Screen coordinate of C
 * @param {Object} pD Screen coordinate of D
 * @param {number} zc Target contour elevation level
 * @returns {Array<Object>} List of crossing points [{x, y}, {x, y}] or empty if no segment crossings
 */
export function getContourSegments(zA, zB, zC, zD, pA, pB, pC, pD, zc) {
    const quadVerts = [
        { z: zA, p: pA },
        { z: zB, p: pB },
        { z: zC, p: pC },
        { z: zD, p: pD }
    ];
    const crossings = [];
    for (let i = 0; i < 4; i++) {
        const v0 = quadVerts[i];
        const v1 = quadVerts[(i + 1) % 4];
        if ((v0.z < zc && v1.z >= zc) || (v1.z < zc && v0.z >= zc)) {
            const t = (zc - v0.z) / ((v1.z - v0.z) || 0.001);
            crossings.push({
                x: v0.p.x + t * (v1.p.x - v0.p.x),
                y: v0.p.y + t * (v1.p.y - v0.p.y)
            });
        }
    }
    return crossings;
}

/**
 * Traces Lagrangian streamlines from velocity vector fields.
 * Useful for animating flow paths and indicating water velocities in channels.
 * @param {Array<Array<number>>} grid_vx 2D velocity array along X
 * @param {Array<Array<number>>} grid_vy 2D velocity array along Y
 * @param {Array<Array<number>>} grid_h 2D water depth array
 * @param {number} size Grid resolution
 * @param {Object} options Configuration parameters for the tracer integration
 * @param {number} [options.stride=2] Stride to sample seed points
 * @param {number} [options.h_threshold=0.002] Minimum water depth to trace
 * @param {number} [options.speed_threshold=0.005] Minimum speed to initiate trace seed
 * @param {number} [options.max_streamlines=60] Max streamlines to return
 * @param {number} [options.max_steps=45] Max steps of integration per streamline
 * @param {number} [options.step_size=0.35] Integration step size
 * @param {number} [options.stop_speed=0.002] Velocity magnitude to cease tracing
 * @returns {Array<Array<Object>>} List of streamlines, where each streamline is an array of {x, y} path points
 */
export function traceStreamlines(grid_vx, grid_vy, grid_h, size, options = {}) {
    const activeStreamlines = [];
    if (!grid_vx || !grid_vy || !grid_h) return activeStreamlines;

    const seeds = [];
    const stride = options.stride !== undefined ? options.stride : 2;
    const h_threshold = options.h_threshold !== undefined ? options.h_threshold : 0.002;
    const speed_threshold = options.speed_threshold !== undefined ? options.speed_threshold : 0.005;
    const max_streamlines = options.max_streamlines !== undefined ? options.max_streamlines : 60;
    const max_steps = options.max_steps !== undefined ? options.max_steps : 45;
    const step_size = options.step_size !== undefined ? options.step_size : 0.35;
    const stop_speed = options.stop_speed !== undefined ? options.stop_speed : 0.002;

    for (let y = 1; y < size - 1; y += stride) {
        for (let x = 1; x < size - 1; x += stride) {
            const h = grid_h[y][x];
            const vx = grid_vx[y][x];
            const vy = grid_vy[y][x];
            const speed = Math.sqrt(vx * vx + vy * vy);
            if (h > h_threshold && speed > speed_threshold) {
                seeds.push({ x, y, speed });
            }
        }
    }

    seeds.sort((a, b) => b.speed - a.speed);

    const limit = Math.min(seeds.length, max_streamlines);
    for (let k = 0; k < limit; k++) {
        const seed = seeds[k];
        let px = seed.x + 0.5;
        let py = seed.y + 0.5;
        const path = [{ x: px, y: py }];
        
        for (let step = 0; step < max_steps; step++) {
            const cx = Math.floor(px);
            const cy = Math.floor(py);
            if (cx < 0 || cx >= size || cy < 0 || cy >= size) break;

            const vx = grid_vx[cy][cx];
            const vy = grid_vy[cy][cx];
            const speed = Math.sqrt(vx * vx + vy * vy);
            if (speed < stop_speed) break;

            px += (vx / speed) * step_size;
            py += (vy / speed) * step_size;
            path.push({ x: px, y: py });
        }

        if (path.length > 3) {
            activeStreamlines.push(path);
        }
    }
    return activeStreamlines;
}

/**
 * Controller class to manage step state, timeline play/pause animations,
 * timeline sliders, and telemetry hover indicators for the dashboard.
 */
export class VisualizerDashboard {
    /**
     * @param {Object} options Parameters for dashboard setup
     * @param {Array<Object>} options.steps Array of simulation state steps
     * @param {number} [options.intervalMs=180] Time between playback steps in milliseconds
     * @param {string} [options.animateBtnId='animate-btn'] Play/pause button DOM element ID
     * @param {string} [options.showFinalBtnId='show-final-btn'] Skip-to-final button DOM element ID
     * @param {string} [options.simPhaseTextId='sim-phase'] Text span showing current simulation era/phase
     * @param {string} [options.telStepId='tel-step'] Text span displaying current step index
     * @param {string} [options.visContainerId='vis-container'] Visual container element for mouse hovers
     * @param {function(number)} options.onRender Callback executed when rendering a frame (receives index)
     * @param {function(number, number, number, number)} [options.onHover] Callback on hover (receives px, py, width, height)
     */
    constructor(options = {}) {
        this.steps = options.steps || [];
        this.stepIndex = 0;
        this.totalSteps = this.steps.length - 1;
        this.animationInterval = null;
        this.intervalMs = options.intervalMs || 180;
        
        this.animateBtn = document.getElementById(options.animateBtnId || 'animate-btn');
        this.showFinalBtn = document.getElementById(options.showFinalBtnId || 'show-final-btn');
        this.simPhaseText = document.getElementById(options.simPhaseTextId || 'sim-phase');
        this.telStep = document.getElementById(options.telStepId || 'tel-step');
        this.visContainer = document.getElementById(options.visContainerId || 'vis-container');

        this.onRender = options.onRender || (() => {});
        this.onHover = options.onHover;

        this.init();
    }

    init() {
        this.updateStep(0);
        
        if (this.visContainer && this.onHover) {
            this.visContainer.addEventListener('mousemove', (e) => {
                const gridRect = this.visContainer.getBoundingClientRect();
                const px = e.clientX - gridRect.left;
                const py = e.clientY - gridRect.top;
                this.onHover(px, py, gridRect.width, gridRect.height);
            });
        }
        if (this.animateBtn) {
            this.animateBtn.addEventListener('click', () => this.togglePlay());
        }
        if (this.showFinalBtn) {
            this.showFinalBtn.addEventListener('click', () => this.showFinalState());
        }
    }

    updateStep(idx) {
        this.stepIndex = idx;
        const stepData = this.steps[idx];
        if (this.telStep && stepData) {
            this.telStep.textContent = stepData.step;
        }
        if (this.simPhaseText && stepData) {
            this.simPhaseText.textContent = `Era: ${stepData.phase || 'Simulation'}`;
        }
        this.onRender(idx);
    }

    togglePlay() {
        if (this.animationInterval) {
            clearInterval(this.animationInterval);
            this.animationInterval = null;
            if (this.animateBtn) {
                this.animateBtn.textContent = 'Play Evolution';
                this.animateBtn.classList.remove('secondary');
            }
        } else {
            if (this.stepIndex >= this.totalSteps) {
                this.stepIndex = 0;
                this.updateStep(0);
            }
            if (this.animateBtn) {
                this.animateBtn.textContent = 'Pause Evolution';
                this.animateBtn.classList.add('secondary');
            }
            this.animationInterval = setInterval(() => {
                let next = this.stepIndex + 1;
                if (next > this.totalSteps) {
                    this.togglePlay();
                    return;
                }
                this.updateStep(next);
            }, this.intervalMs);
        }
    }

    showFinalState() {
        if (this.animationInterval) this.togglePlay();
        this.updateStep(this.totalSteps);
    }
}
