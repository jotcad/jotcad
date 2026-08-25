#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct TransformOpBase : P {
    static void execute_multi(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Matrix>& result_tfs) {
        if (result_tfs.size() == 1) {
            Shape out = in;
            out.apply_transform(result_tfs[0] * out.tf.inverse());
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        Shape out;
        out.tf = Matrix::identity();
        out.add_tag("type", "group");
        for (const auto& m : result_tfs) {
            Shape c = in;
            c.apply_transform(m * c.tf.inverse());
            out.components.push_back(c);
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct ByOp : TransformOpBase<P> {
    static constexpr const char* path = "jot/by";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& targets) {
        if (targets.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        if (targets.size() == 1) {
            Shape out = in;
            out.apply_transform(targets[0].tf);
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        Shape out;
        out.tf = Matrix::identity();
        out.add_tag("type", "group");
        for (const auto& t : targets) {
            Shape c = in;
            c.apply_transform(t.tf);
            out.components.push_back(c);
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "targets"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/by"},
            {"description", "Transforms the subject by the matrices of the target shapes. Supports sequences."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "targets"}, {"type", "jot:shapes"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ToOp : TransformOpBase<P> {
    static constexpr const char* path = "jot/to";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& targets) {
        if (targets.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        std::vector<Matrix> tfs;
        for (const auto& t : targets) {
            tfs.push_back(t.tf);
        }
        TransformOpBase<P>::execute_multi(vfs, fulfilling, in, tfs);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "targets"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/to"},
            {"description", "Moves the subject to the frames of the target shapes (resets local transform). Supports sequences."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "targets"}, {"type", "jot:shapes"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct DupOp : TransformOpBase<P> {
    static constexpr const char* path = "jot/dup";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double count) {
        int c = (int)count;
        if (c <= 0) {
            Shape out;
            out.tf = Matrix::identity();
            out.add_tag("type", "group");
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }
        if (c == 1) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        std::vector<Matrix> tfs;
        for (int i = 0; i < c; ++i) {
            tfs.push_back(in.tf);
        }
        TransformOpBase<P>::execute_multi(vfs, fulfilling, in, tfs);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "count"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/dup"},
            {"description", "Duplicates the subject 'count' times in place."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "count"}, {"type", "jot:number"}, {"default", 1.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct OriginOp : P {
    static constexpr const char* path = "jot/origin";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, std::optional<Shape> in) {
        if (in.has_value()) {
            Shape out = in.value();
            out.tf = in.value().tf.inverse();
            vfs->write(fulfilling.with_output("$out"), out);
        } else {
            Shape out;
            out.tf = Matrix::identity();
            vfs->write(fulfilling.with_output("$out"), out);
        }
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/origin"},
            {"description", "Resets the subject's transformation to the identity matrix (birth origin), or returns an identity frame if no subject is provided."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"optional", true}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct GapOp : P {
    static constexpr const char* path = "jot/gap";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = in;
        out.set_role_recursive("gap");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/gap"},
            {"description", "Tags the subject as a 'gap' (negative space). Gaps cut into other shapes but are themselves non-matter. They are rendered transparently in the UX and skipped during STL export."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct GhostOp : P {
    static constexpr const char* path = "jot/ghost";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = in;
        out.set_role_recursive("ghost");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/ghost"},
            {"description", "Tags the subject as a 'ghost' (visual reference). Ghosts are ignored by Boolean kernels and promoters."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct MarkOp : P {
    static constexpr const char* path = "jot/mark";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = in;
        out.set_role_recursive("mark");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/mark"},
            {"description", "Tags the subject as a 'mark' (annotation). Marks are ignored by Boolean kernels but included in PDF exports."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct MaskOp : P {
    static constexpr const char* path = "jot/mask";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = in;
        out.set_role_recursive("mask");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/mask"},
            {"description", "Tags the subject as a 'mask' (logical filter). Masks are ignored by Boolean kernels and are invisible in standard renders."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct CenterOp : P {
    static constexpr const char* path = "jot/center";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, std::optional<Shape> in) {
        if (!in.has_value()) {
            Shape out;
            out.tf = Matrix::identity();
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        const Shape& subject = in.value();
        CGAL::Bbox_3 bbox;
        bool first = true;

        std::vector<const Shape*> stack = {&subject};
        while (!stack.empty()) {
            const Shape* curr = stack.back();
            stack.pop_back();

            if (curr->geometry.has_value()) {
                Geometry geo = vfs->read<Geometry>(*curr->geometry);
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
            for (const auto& child : curr->components) {
                stack.push_back(&child);
            }
        }

        double cx = 0, cy = 0, cz = 0;
        if (!first) {
            cx = (CGAL::to_double(bbox.xmin()) + CGAL::to_double(bbox.xmax())) / 2.0;
            cy = (CGAL::to_double(bbox.ymin()) + CGAL::to_double(bbox.ymax())) / 2.0;
            cz = (CGAL::to_double(bbox.zmin()) + CGAL::to_double(bbox.zmax())) / 2.0;
        } else {
            Point_3 origin = subject.tf.transform(Point_3(0, 0, 0));
            cx = CGAL::to_double(origin.x());
            cy = CGAL::to_double(origin.y());
            cz = CGAL::to_double(origin.z());
        }

        Shape out;
        out.tf = Matrix::translate(FT(cx), FT(cy), FT(cz));
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/center"},
            {"description", "Returns a spatial frame shape located at the spatial centroid/center of the subject."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"optional", true}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct AlignOp : TransformOpBase<P> {
    static constexpr const char* path = "jot/align";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& targets) {
        if (targets.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        if (targets.size() == 1) {
            Shape out = in;
            out.apply_transform(targets[0].tf.inverse());
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        Shape out;
        out.tf = Matrix::identity();
        out.add_tag("type", "group");
        for (const auto& t : targets) {
            Shape c = in;
            c.apply_transform(t.tf.inverse());
            out.components.push_back(c);
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "targets"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/align"},
            {"description", "Aligns the subject by the inverse matrices of the target shapes (shorthand for a.by(b.origin())). Supports sequences."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "targets"}, {"type", "jot:shapes"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct AlignXOp : TransformOpBase<P> {
    static constexpr const char* path = "jot/alignX";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& targets) {
        if (targets.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Shape out = in;
        Matrix inv = targets[0].tf.inverse();
        FT tx = inv.t.m(0, 3);
        out.apply_transform(Matrix::translate(tx, FT(0), FT(0)));
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "targets"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/alignX"},
            {"description", "Aligns the subject along X axis only using target shape matrix."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({{{"name", "targets"}, {"type", "jot:shapes"}}})},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct AlignYOp : TransformOpBase<P> {
    static constexpr const char* path = "jot/alignY";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& targets) {
        if (targets.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Shape out = in;
        Matrix inv = targets[0].tf.inverse();
        FT ty = inv.t.m(1, 3);
        out.apply_transform(Matrix::translate(FT(0), ty, FT(0)));
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "targets"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/alignY"},
            {"description", "Aligns the subject along Y axis only using target shape matrix."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({{{"name", "targets"}, {"type", "jot:shapes"}}})},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct AlignZOp : TransformOpBase<P> {
    static constexpr const char* path = "jot/alignZ";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& targets) {
        if (targets.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Shape out = in;
        Matrix inv = targets[0].tf.inverse();
        FT tz = inv.t.m(2, 3);
        out.apply_transform(Matrix::translate(FT(0), FT(0), tz));
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "targets"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/alignZ"},
            {"description", "Aligns the subject along Z axis only using target shape matrix."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({{{"name", "targets"}, {"type", "jot:shapes"}}})},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct PointAnchorOp : P {
    static constexpr const char* path = "jot/point";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& subject, const std::string& key) {
        CGAL::Bbox_3 bbox;
        bool first = true;
        std::vector<const Shape*> stack;
        stack.push_back(&subject);

        while (!stack.empty()) {
            const Shape* curr = stack.back();
            stack.pop_back();

            if (curr->geometry.has_value()) {
                Geometry geo = vfs->read<Geometry>(*curr->geometry);
                for (const auto& v : geo.vertices) {
                    Point_3 p = curr->tf.transform(Point_3(v.x, v.y, v.z));
                    if (first) { bbox = p.bbox(); first = false; }
                    else { bbox += p.bbox(); }
                }
            }
            for (const auto& child : curr->components) stack.push_back(&child);
        }

        double x = 0, y = 0, z = 0;
        if (!first) {
            double xmin = CGAL::to_double(bbox.xmin()), xmax = CGAL::to_double(bbox.xmax());
            double ymin = CGAL::to_double(bbox.ymin()), ymax = CGAL::to_double(bbox.ymax());
            double zmin = CGAL::to_double(bbox.zmin()), zmax = CGAL::to_double(bbox.zmax());
            double xmid = (xmin + xmax) / 2.0, ymid = (ymin + ymax) / 2.0, zmid = (zmin + zmax) / 2.0;

            if (key == "center") { x = xmid; y = ymid; z = zmid; }
            else if (key == "origin") { Point_3 o = subject.tf.transform(Point_3(0,0,0)); x = CGAL::to_double(o.x()); y = CGAL::to_double(o.y()); z = CGAL::to_double(o.z()); }
            else if (key == "bld" || key == "xmin-ymin-zmin" || key == "bottom-left-down") { x = xmin; y = ymin; z = zmin; }
            else if (key == "blu" || key == "xmin-ymin-zmax" || key == "bottom-left-up") { x = xmin; y = ymin; z = zmax; }
            else if (key == "brd" || key == "xmax-ymin-zmin" || key == "bottom-right-down") { x = xmax; y = ymin; z = zmin; }
            else if (key == "bru" || key == "xmax-ymin-zmax" || key == "bottom-right-up") { x = xmax; y = ymin; z = zmax; }
            else if (key == "tld" || key == "xmin-ymax-zmin" || key == "top-left-down") { x = xmin; y = ymax; z = zmin; }
            else if (key == "tlu" || key == "xmin-ymax-zmax" || key == "top-left-up") { x = xmin; y = ymax; z = zmax; }
            else if (key == "trd" || key == "xmax-ymax-zmin" || key == "top-right-down") { x = xmax; y = ymax; z = zmin; }
            else if (key == "tru" || key == "xmax-ymax-zmax" || key == "top-right-up") { x = xmax; y = ymax; z = zmax; }
            else { x = xmid; y = ymid; z = zmid; }
        } else {
            Point_3 o = subject.tf.transform(Point_3(0,0,0));
            x = CGAL::to_double(o.x()); y = CGAL::to_double(o.y()); z = CGAL::to_double(o.z());
        }

        Shape out;
        out.tf = Matrix::translate(FT(x), FT(y), FT(z));
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "key"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/point"},
            {"description", "Returns a 0D spatial frame point anchor (center, origin, bld, tru, etc.)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({{{"name", "key"}, {"type", "jot:string"}, {"default", "center"}}})},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void transform_ops_init(fs::VFSNode* vfs) {
    Processor::register_op<ByOp<>, Shape, std::vector<Shape>>(vfs, "jot/by");
    Processor::register_op<AlignOp<>, Shape, std::vector<Shape>>(vfs, "jot/align");
    Processor::register_op<AlignXOp<>, Shape, std::vector<Shape>>(vfs, "jot/alignX");
    Processor::register_op<AlignYOp<>, Shape, std::vector<Shape>>(vfs, "jot/alignY");
    Processor::register_op<AlignZOp<>, Shape, std::vector<Shape>>(vfs, "jot/alignZ");
    Processor::register_op<ToOp<>, Shape, std::vector<Shape>>(vfs, "jot/to");
    Processor::register_op<DupOp<>, Shape, double>(vfs, "jot/dup");
    Processor::register_op<GapOp<>, Shape>(vfs, "jot/gap");
    Processor::register_op<GhostOp<>, Shape>(vfs, "jot/ghost");
    Processor::register_op<MarkOp<>, Shape>(vfs, "jot/mark");
    Processor::register_op<MaskOp<>, Shape>(vfs, "jot/mask");
    Processor::register_op<OriginOp<>, std::optional<Shape>>(vfs, "jot/origin");
    Processor::register_op<OriginOp<>, std::optional<Shape>>(vfs, "jot/o");
    Processor::register_op<CenterOp<>, std::optional<Shape>>(vfs, "jot/center");
    Processor::register_op<PointAnchorOp<>, Shape, std::string>(vfs, "jot/point");
}

} // namespace geo
} // namespace jotcad
