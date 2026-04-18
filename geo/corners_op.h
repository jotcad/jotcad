#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct CornersOp : P {
    static constexpr const char* path = "jot/corners";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, bool proxy, Shape& out) {
        Geometry geo = vfs->template read<Geometry>({in.geometry.path, in.geometry.parameters});
        std::vector<Shape> components;

        for (const auto& face : geo.faces) {
            for (const auto& loop : face.loops) {
                if (loop.size() < 2) continue;
                
                for (size_t i = 0; i < loop.size(); ++i) {
                    int idx_curr = loop[i];
                    int idx_next = loop[(i + 1) % loop.size()];
                    int idx_prev = loop[(i + loop.size() - 1) % loop.size()];

                    auto v_curr = geo.vertices[idx_curr];
                    auto v_next = geo.vertices[idx_next];
                    auto v_prev = geo.vertices[idx_prev];

                    // Vectors in global space
                    double dx = v_next.x - v_curr.x;
                    double dy = v_next.y - v_curr.y;
                    double dz = v_next.z - v_curr.z;
                    double len_next = std::sqrt(dx*dx + dy*dy + dz*dz);
                    
                    if (len_next < 1e-9) continue;

                    // X Axis = Unit Outgoing Edge
                    double ux = dx / len_next;
                    double uy = dy / len_next;
                    double uz = dz / len_next;

                    // Z Axis = Face Normal (Approximated from triangle if needed, here Z0 for 2D)
                    // TODO: For 3D, use Newell's or cross product of legs.
                    double nx = 0, ny = 0, nz = 1; 

                    // Y Axis = Z cross X
                    double vy = nz * ux - nx * uz;
                    double vx = ny * uz - nz * uy;
                    double vz = nx * uy - ny * ux;

                    // Local-to-Global Matrix
                    Matrix m;
                    m.data[0] = ux; m.data[1] = vx; m.data[2] = nx; m.data[3] = v_curr.x;
                    m.data[4] = uy; m.data[5] = vy; m.data[6] = ny; m.data[7] = v_curr.y;
                    m.data[8] = uz; m.data[9] = vz; m.data[10] = nz; m.data[11] = v_curr.z;

                    Shape corner;
                    corner.tf = m.to_vec();
                    corner.add_tag("type", "corner");
                    corner.add_tag("index", (int)i);

                    if (proxy) {
                        // V-Proxy at Origin
                        Geometry v_geo;
                        v_geo.vertices.push_back({0, 0, 0}); // Corner
                        v_geo.vertices.push_back({len_next, 0, 0}); // Outgoing Leg
                        
                        // Transform Incoming Leg to local space
                        double p_dx = v_prev.x - v_curr.x;
                        double p_dy = v_prev.y - v_curr.y;
                        double p_dz = v_prev.z - v_curr.z;
                        
                        // Inverse Matrix multiplication (Point mapping)
                        Matrix inv = m.inverse();
                        double lx = inv.data[0]*p_dx + inv.data[1]*p_dy + inv.data[2]*p_dz;
                        double ly = inv.data[4]*p_dx + inv.data[5]*p_dy + inv.data[6]*p_dz;
                        double lz = inv.data[8]*p_dx + inv.data[9]*p_dy + inv.data[10]*p_dz;
                        v_geo.vertices.push_back({lx, ly, lz});

                        v_geo.segments.push_back({0, 1});
                        v_geo.segments.push_back({0, 2});
                        corner.geometry = vfs->write_geometry(v_geo);
                    }

                    components.push_back(corner);
                }
            }
        }

        out.geometry.path = "op/group";
        out.components = components;
        out.add_tag("type", "corners");
    }

    static std::vector<std::string> argument_keys() { return {"$in", "proxy"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"proxy", {{"type", "jot:boolean"}, {"default", true}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void corners_init() {
    Processor::register_op<CornersOp<>, Shape, Shape, bool>();
}

} // namespace geo
} // namespace jotcad
