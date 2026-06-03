#pragma once
#include "protocols.h"
#include "processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct MaterialOp : P {
    static constexpr const char* path = "jot/material";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& material) {
        Shape out = in;
        out.tags["material"] = material;
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "material"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/material"},
            {"description", "Applies a material tag to the input shape."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"description", "The shape to apply material to."}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "material"}, {"type", "jot:string"}, {"description", "The material name or texture CID."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The material-tagged shape."}}}}}
        };
    }
};

static void material_init(fs::VFSNode* vfs) {
    Processor::register_op<MaterialOp<>, Shape, std::string>(vfs, "jot/material");
}

} // namespace geo
} // namespace jotcad
