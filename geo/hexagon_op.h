#pragma once
#include "impl/processor.h"
#include "impl/hexagon.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> hexagon_op(jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack = {}) {
    std::cout << "[Hexagon Op] Generating hexagon with params: " << params.dump() << std::endl;
    double radius = params.value("radius", 10.0);
    std::string variant = params.value("variant", "full");

    Geometry geo;
    makeHexagon(geo, radius, variant);

    // 1. Write the raw mesh to a content-addressed location (geo/mesh)
    std::string mesh_text = geo.encode_text();
    std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
    std::cout << "[Hexagon Op] Provisioning geo/mesh for params: " << params.dump() << " Stack: ";
    for (const auto& s : stack) std::cout << s << " -> ";
    std::cout << std::endl;
    
    vfs->write("geo/mesh", params, mesh_data);

    // 2. Return the Shape JSON for the requested path
    nlohmann::json shape = {
        {"geometry", "vfs:/geo/mesh"},
        {"parameters", params},
        {"tags", {{"type", "hexagon"}, {"variant", variant}}}
    };
    std::string shape_text = shape.dump(); // Use compact dump to match JS
    std::cout << "[Hexagon Op] Returning Shape JSON for " << path << ": " << shape_text << std::endl;
    return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
}

static void hexagon_init() {
    Processor::Operation op;
    op.path = "shape/hexagon";
    op.logic = hexagon_op;
    op.schema = {
        {"type", "object"},
        {"ux", "vfs:/ui/components/hexagon"},
        {"componentName", "jotcad-hexagon-ux"},
        {"properties", {
            {"radius", {{"type", "number"}, {"default", 10}}},
            {"variant", {{"type", "string"}, {"enum", {"full", "half", "middle", "cap", "sector"}}, {"default", "full"}}}
        }}
    };
    op.ux_path = "ui/components/hexagon";
    op.ux_code = R"(
class HexagonEditor extends HTMLElement {
    constructor() {
        super();
        this.attachShadow({ mode: 'open' });
    }
    connectedCallback() { this.render(); }
    static get observedAttributes() { return ['radius', 'variant']; }
    attributeChangedCallback() { this.render(); }
    render() {
        const r = this.getAttribute('radius') || 10;
        const v = this.getAttribute('variant') || 'full';
        this.shadowRoot.innerHTML = `
            <style>
                .container { display: flex; flex-direction: column; gap: 8px; padding: 12px; background: rgba(255,255,255,0.05); border-radius: 8px; }
                label { display: flex; justify-content: space-between; font-size: 11px; font-family: monospace; color: #cbd5e1; align-items: center; }
                input[type="range"], select { flex: 1; margin-left: 8px; cursor: pointer; accent-color: #f97316; background: #1e293b; color: white; border: 1px solid #334155; border-radius: 4px; font-size: 10px; }
                span { width: 32px; text-align: right; font-weight: bold; color: white; }
                .header { font-size: 10px; font-weight: bold; text-transform: uppercase; letter-spacing: 2px; color: #f97316; margin-bottom: 4px; }
            </style>
            <div class="container">
                <div class="header">Hexagon Agent UI</div>
                <label>Radius <span>${r}</span> <input type="range" id="r" min="1" max="100" value="${r}" /></label>
                <label>Variant 
                    <select id="v">
                        <option value="full" ${v === 'full' ? 'selected' : ''}>Full</option>
                        <option value="half" ${v === 'half' ? 'selected' : ''}>Half</option>
                        <option value="middle" ${v === 'middle' ? 'selected' : ''}>Middle (Rect)</option>
                        <option value="cap" ${v === 'cap' ? 'selected' : ''}>Cap (Triangle)</option>
                        <option value="sector" ${v === 'sector' ? 'selected' : ''}>Sector (1/6)</option>
                    </select>
                </label>
            </div>
        `;
        ['r', 'v'].forEach(id => {
            this.shadowRoot.getElementById(id).addEventListener('input', (e) => {
                this.dispatchEvent(new CustomEvent('param-change', {
                    detail: {
                        radius: Number(this.shadowRoot.getElementById('r').value),
                        variant: this.shadowRoot.getElementById('v').value
                    },
                    bubbles: true, composed: true
                }));
            });
        });
    }
}
customElements.define('jotcad-hexagon-ux', HexagonEditor);
)";
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
