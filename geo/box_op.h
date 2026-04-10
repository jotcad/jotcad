#pragma once
#include "impl/processor.h"
#include "impl/box.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> box_op(jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params) {
    std::cout << "[Box Op] Generating box with params: " << params.dump() << std::endl;
    double w = params.value("width", 1.0);
    double h = params.value("height", 1.0);
    double d = params.value("depth", 1.0);

    Geometry geo;
    makeBox(geo, w, h, d);

    // 1. Write the raw mesh to a content-addressed location (geo/mesh)
    std::string mesh_text = geo.encode_text();
    std::cout << "[Box Op] Generated mesh, " << mesh_text.size() << " bytes. Writing to geo/mesh..." << std::endl;
    std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
    vfs->write("geo/mesh", params, mesh_data);

    // 2. Return the Shape JSON for the requested path
    nlohmann::json shape = {
        {"geometry", "vfs:/geo/mesh"},
        {"parameters", params},
        {"tags", {{"type", "box"}}}
    };
    std::string shape_text = shape.dump(2);
    std::cout << "[Box Op] Returning Shape JSON for " << path << std::endl;
    return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
}

static void box_init() {
    Processor::Operation op;
    op.path = "shape/box";
    op.logic = box_op;
    op.schema = {
        {"type", "object"},
        {"ux", "vfs:/ui/components/box"},
        {"componentName", "jotcad-box-ux"},
        {"properties", {
            {"width", {{"type", "number"}, {"default", 10}}},
            {"height", {{"type", "number"}, {"default", 10}}},
            {"depth", {{"type", "number"}, {"default", 10}}}
        }}
    };
    op.ux_path = "ui/components/box";
    op.ux_code = R"(
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
        this.shadowRoot.innerHTML = `
            <style>
                .container { display: flex; flex-direction: column; gap: 8px; padding: 12px; background: rgba(255,255,255,0.05); border-radius: 8px; }
                label { display: flex; justify-content: space-between; font-size: 11px; font-family: monospace; color: #cbd5e1; align-items: center; }
                input { flex: 1; margin-left: 8px; cursor: pointer; accent-color: #f97316; }
                span { width: 24px; text-align: right; font-weight: bold; color: white; }
                .header { font-size: 10px; font-weight: bold; text-transform: uppercase; letter-spacing: 2px; color: #f97316; margin-bottom: 4px; }
            </style>
            <div class="container">
                <div class="header">Box Agent UI</div>
                <label>W <span>${w}</span> <input type="range" id="w" min="1" max="100" value="${w}" /></label>
                <label>H <span>${h}</span> <input type="range" id="h" min="1" max="100" value="${h}" /></label>
                <label>D <span>${d}</span> <input type="range" id="d" min="1" max="100" value="${d}" /></label>
            </div>
        `;
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
)";
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
