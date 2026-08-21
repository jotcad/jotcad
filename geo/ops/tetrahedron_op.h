#pragma once
#include "protocols.h"
#include "processor.h"
#include "geometry.h"
#include <cmath>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct TetrahedronOp : P {
    static constexpr const char* path = "jot/Tetrahedron";

    static void execute_tetrahedron(fs::VFSNode* vfs, const fs::Selector& fulfilling, double size) {
        Geometry res;
        double s = (size <= 0.0) ? 10.0 : size;

        // Exact geometric proportions for regular tetrahedron centered on centroid (0,0,0)
        // Height H = s * sqrt(2/3)
        // Circumradius R = z_apex = s * sqrt(6)/4
        // z_base = -H/4 = -s * sqrt(6)/12
        // r_base = s * sqrt(3)/3
        double sqrt3 = std::sqrt(3.0);
        double sqrt6 = std::sqrt(6.0);

        double z_apex = (sqrt6 / 4.0) * s;
        double z_base = -(sqrt6 / 12.0) * s;
        double r_top = (sqrt3 / 3.0) * s;
        double r_side = (sqrt3 / 6.0) * s;
        double s_half = s / 2.0;

        // Vertices
        int v0 = (int)res.vertices.size(); // Base vertex 0 (+Y)
        res.vertices.push_back({FT(0.0), FT(r_top), FT(z_base)});

        int v1 = (int)res.vertices.size(); // Base vertex 1 (-X, -Y)
        res.vertices.push_back({FT(-s_half), FT(-r_side), FT(z_base)});

        int v2 = (int)res.vertices.size(); // Base vertex 2 (+X, -Y)
        res.vertices.push_back({FT(s_half), FT(-r_side), FT(z_base)});

        int v3 = (int)res.vertices.size(); // Apex vertex (+Z)
        res.vertices.push_back({FT(0.0), FT(0.0), FT(z_apex)});

        // 4 Triangular Faces with outward normal (CCW) winding
        // 1. Bottom Face (-Z)
        res.faces.push_back({{{v0, v2, v1}}});

        // 2. Side Face 0 (v0, v1, v3)
        res.faces.push_back({{{v0, v1, v3}}});

        // 3. Side Face 1 (v1, v2, v3)
        res.faces.push_back({{{v1, v2, v3}}});

        // 4. Side Face 2 (v2, v0, v3)
        res.faces.push_back({{{v2, v0, v3}}});

        res.triangulate();

        Shape out = P::make_shape(vfs, res, {{"type", "closed"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    struct Standard {
        static constexpr const char* path = "jot/Tetrahedron";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double size) {
            execute_tetrahedron(vfs, fulfilling, size);
        }
        static std::vector<std::string> argument_keys() { return {"size"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates a regular 3D tetrahedron centered on its centroid."},
                {"inputs", nlohmann::json::object()},
                {"arguments", nlohmann::json::array({
                    {{"name", "size"}, {"type", "jot:number"}, {"default", 10.0}}
                })},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };
};

static void tetrahedron_init(fs::VFSNode* vfs) {
    Processor::register_op<TetrahedronOp<>::Standard, double>(vfs, "jot/Tetrahedron");
}

} // namespace geo
} // namespace jotcad
