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
        out.add_tag("role", "gap");
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
        out.add_tag("role", "ghost");
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
        out.add_tag("role", "mark");
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
        out.add_tag("role", "mask");
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

inline void transform_ops_init(fs::VFSNode* vfs) {
    Processor::register_op<ByOp<>, Shape, std::vector<Shape>>(vfs, "jot/by");
    Processor::register_op<ToOp<>, Shape, std::vector<Shape>>(vfs, "jot/to");
    Processor::register_op<DupOp<>, Shape, double>(vfs, "jot/dup");
    Processor::register_op<GapOp<>, Shape>(vfs, "jot/gap");
    Processor::register_op<GhostOp<>, Shape>(vfs, "jot/ghost");
    Processor::register_op<MarkOp<>, Shape>(vfs, "jot/mark");
    Processor::register_op<MaskOp<>, Shape>(vfs, "jot/mask");
    Processor::register_op<OriginOp<>, std::optional<Shape>>(vfs, "jot/origin");
    Processor::register_op<OriginOp<>, std::optional<Shape>>(vfs, "jot/o");
}

} // namespace geo
} // namespace jotcad
