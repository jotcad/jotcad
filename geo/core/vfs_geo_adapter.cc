#include "vfs_node.h"
#include "geometry.h"
#include "shape.h"
#include "../math/matrix.h"
#include "../render/triangulation.h"

namespace jotcad {
namespace geo {

void Geometry::apply_tf(const Matrix& m) {
    for (auto& v : vertices) {
        EK::Point_3 p(v.x, v.y, v.z);
        EK::Point_3 tp = m.transform(p);
        v.x = tp.x(); v.y = tp.y(); v.z = tp.z();
    }
    // Involution: If reflection, flip faces and triangles to preserve outwards normals.
    if (m.is_reflection()) {
        for (auto& face : faces) {
            for (auto& loop : face.loops) {
                std::reverse(loop.begin(), loop.end());
            }
        }
        for (auto& t : triangles) {
            std::swap(t[1], t[2]);
        }
    }
}

void Geometry::triangulate() {
    if (faces.empty()) return;
    triangles.clear();

    std::vector<Vec3> pts;
    for (const auto& v : vertices) {
        pts.push_back({CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)});
    }

    for (const auto& face : faces) {
        if (face.loops.empty()) continue;
        
        if (face.loops.size() == 1 && face.loops[0].size() == 3) {
            // Optimization: Simple 3-sided face with no holes
            triangles.push_back({face.loops[0][0], face.loops[0][1], face.loops[0][2]});
        } else {
            // Complex face or PWH: Use CDT triangulation
            Triangulation::triangulate_face(face, pts, [&](int i0, int i1, int i2) {
                triangles.push_back({i0, i1, i2});
            });
        }
    }
}

} // namespace geo
} // namespace jotcad

namespace fs {

// --- read(Selector) ---

template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const Selector& sel) {
    auto data = read<std::vector<uint8_t>>(sel);
    std::string text(data.begin(), data.end());
    jotcad::geo::Geometry g;
    g.decode_text(text);
    return g;
}

template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const Selector& sel) {
    auto j = this->read<nlohmann::json>(sel);
    return jotcad::geo::Shape::from_json(j);
}

// --- read(VFSRequest) ---

template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const VFSRequest& req) {
    auto data = read<std::vector<uint8_t>>(req);
    std::string text(data.begin(), data.end());
    jotcad::geo::Geometry g;
    g.decode_text(text);
    return g;
}

template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const VFSRequest& req) {
    auto j = this->read<nlohmann::json>(req);
    return jotcad::geo::Shape::from_json(j);
}

// --- read(CID) ---

template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const CID& cid) {
    auto data = read<std::vector<uint8_t>>(cid);
    std::string text(data.begin(), data.end());
    jotcad::geo::Geometry g;
    g.decode_text(text);
    return g;
}

template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const CID& cid) {
    auto j = this->read<nlohmann::json>(cid);
    return jotcad::geo::Shape::from_json(j);
}

// --- write implementations ---

Selector VFSNode::write(const Selector& sel, const jotcad::geo::Geometry& data) {
    return write(sel, data.encode_text());
}

Selector VFSNode::write(const Selector& sel, const jotcad::geo::Shape& data) {
    return write(sel, data.to_json());
}

template<> CID VFSNode::materialize<jotcad::geo::Geometry>(const jotcad::geo::Geometry& data) {
    jotcad::geo::Geometry copy = data;

    // Fallback Triangulation: Only triangulate if the producer forgot to populate triangles
    if (copy.triangles.empty() && !copy.faces.empty()) {
        copy.triangulate();
    }

    return materialize<std::string>(copy.encode_text());
}

template<> CID VFSNode::materialize<jotcad::geo::Shape>(const jotcad::geo::Shape& data) {
    return materialize<json>(data.to_json());
}

} // namespace fs
