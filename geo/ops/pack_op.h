#pragma once
#include "protocols.h"
#include "processor.h"
#include "pack/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PackOp : P {
    static constexpr const char* path = "jot/Pack";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Shape subject, double width, double height, double spacing, double margin) {
        
        pack::Engine::Config config;
        config.sheet_width = width;
        config.sheet_height = height;
        config.spacing = spacing;
        config.margin = margin;

        // Perform nesting on the components of the subject
        pack::Engine::pack(vfs, subject.components, config);
        
        // Write the modified subject back
        vfs->write(fulfilling.with_output("$out"), subject);
    }
    
    static std::vector<std::string> argument_keys() { 
        return {"on", "width", "height", "spacing", "margin"}; 
    }
    
    static typename P::json schema() {
        return {
            {"path", "jot/Pack"},
            {"description", "Packs the child components of a shape into a 2D sheet."},
            {"arguments", {
                {{"name", "on"}, {"type", "jot:shape"}},
                {{"name", "width"}, {"type", "number"}, {"default", 1000.0}},
                {{"name", "height"}, {"type", "number"}, {"default", 1000.0}},
                {{"name", "spacing"}, {"type", "number"}, {"default", 2.0}},
                {{"name", "margin"}, {"type", "number"}, {"default", 5.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void pack_init(fs::VFSNode* vfs) {
    Processor::register_op<PackOp<>, Shape, double, double, double, double>(vfs, "jot/Pack");
}

} // namespace geo
} // namespace jotcad
