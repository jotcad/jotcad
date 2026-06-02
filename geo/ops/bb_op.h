#pragma once
#include "protocols.h"
#include "processor.h"
#include "box_op.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct BoundingBoxOp : P {
    static constexpr const char* path = "jot/boundingBox";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double grow) {
        CGAL::Bbox_3 bbox;
        bool first = true;

        std::vector<const Shape*> stack = {&in};
        while (!stack.empty()) {
            const Shape* curr = stack.back();
            stack.pop_back();

            if (curr->geometry.has_value()) {
                Geometry geo = vfs->read<Geometry>(*curr->geometry);
                if (!geo.vertices.empty()) {
                    for (const auto& v : geo.vertices) {
                        Point_3 p = curr->tf.transform(Point_3(v.x, v.y, v.z));
                        if (first) {
                            bbox = p.bbox();
                            first = false;
                        } else {
                            bbox += p.bbox();
                        }
                    }
                }
            } 
            
            // Recurse into children
            for (const auto& child : curr->components) {
                stack.push_back(&child);
            }
        }

        if (first) {
            // Empty or no geometry, use subject's origin
            Point_3 p = in.tf.transform(Point_3(0, 0, 0));
            bbox = p.bbox();
        }

        double min_x = CGAL::to_double(bbox.xmin()) - grow;
        double max_x = CGAL::to_double(bbox.xmax()) + grow;
        double min_y = CGAL::to_double(bbox.ymin()) - grow;
        double max_y = CGAL::to_double(bbox.ymax()) + grow;
        double min_z = CGAL::to_double(bbox.zmin()) - grow;
        double max_z = CGAL::to_double(bbox.zmax()) + grow;

        double w = max_x - min_x;
        double d = max_y - min_y;
        double h = max_z - min_z;

        // Generate geometry matching BoxOp's dimensional logic
        Geometry res;
        std::string type = "closed";
        
        bool dx = (w > 1e-9);
        bool dy = (d > 1e-9);
        bool dz = (h > 1e-9);
        int dims = (dx ? 1 : 0) + (dy ? 1 : 0) + (dz ? 1 : 0);

        if (dims == 3) {
            res.vertices.push_back({FT(0), FT(0), FT(0)}); // 0
            res.vertices.push_back({FT(w), FT(0), FT(0)}); // 1
            res.vertices.push_back({FT(w), FT(d), FT(0)}); // 2
            res.vertices.push_back({FT(0), FT(d), FT(0)}); // 3
            res.vertices.push_back({FT(0), FT(0), FT(h)}); // 4
            res.vertices.push_back({FT(w), FT(0), FT(h)}); // 5
            res.vertices.push_back({FT(w), FT(d), FT(h)}); // 6
            res.vertices.push_back({FT(0), FT(d), FT(h)}); // 7

            res.faces.push_back({{{3, 2, 1, 0}}}); // Bottom
            res.faces.push_back({{{4, 5, 6, 7}}}); // Top
            res.faces.push_back({{{0, 1, 5, 4}}}); // Front
            res.faces.push_back({{{1, 2, 6, 5}}}); // Right
            res.faces.push_back({{{2, 3, 7, 6}}}); // Back
            res.faces.push_back({{{3, 0, 4, 7}}}); // Left
            type = "closed";
        } else if (dims == 2) {
            if (!dz) { // XY Plane
                res.vertices.push_back({FT(0), FT(0), FT(0)});
                res.vertices.push_back({FT(w), FT(0), FT(0)});
                res.vertices.push_back({FT(w), FT(d), FT(0)});
                res.vertices.push_back({FT(0), FT(d), FT(0)});
            } else if (!dy) { // XZ Plane
                res.vertices.push_back({FT(0), FT(0), FT(0)});
                res.vertices.push_back({FT(w), FT(0), FT(0)});
                res.vertices.push_back({FT(w), FT(0), FT(h)});
                res.vertices.push_back({FT(0), FT(0), FT(h)});
            } else { // YZ Plane
                res.vertices.push_back({FT(0), FT(0), FT(0)});
                res.vertices.push_back({FT(0), FT(d), FT(0)});
                res.vertices.push_back({FT(0), FT(d), FT(h)});
                res.vertices.push_back({FT(0), FT(0), FT(h)});
            }
            res.faces.push_back({{{0, 1, 2, 3}}});
            type = "surface";
        } else if (dims == 1) {
            res.vertices.push_back({FT(0), FT(0), FT(0)});
            res.vertices.push_back({FT(w), FT(d), FT(h)});
            res.segments.push_back({0, 1});
            type = "segments";
        } else {
            res.vertices.push_back({FT(0), FT(0), FT(0)});
            res.points.push_back(0);
            type = "points";
        }

        Shape out = P::make_shape(vfs, res, {{"type", type}});
        // Set its absolute transform to the min-corner
        out.tf = Matrix::translate(FT(min_x), FT(min_y), FT(min_z));

        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "grow"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/boundingBox"},
            {"description", "Returns a bounding box for the subject. The box starts at the local origin [0,w], [0,d], [0,h] and is transformed to match the subject's bounds."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "grow"}, {"type", "jot:number"}, {"default", 0.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void bb_init(fs::VFSNode* vfs) {
    Processor::register_op<BoundingBoxOp<>, Shape, double>(vfs, "jot/boundingBox");
    Processor::register_op<BoundingBoxOp<>, Shape, double>(vfs, "jot/bb");
}

} // namespace geo
} // namespace jotcad
