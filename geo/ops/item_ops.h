#pragma once
#include "protocols.h"
#include "processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ItemOp : P {
    static constexpr const char* path = "jot/item";
    
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, std::string name, const std::vector<Shape>& shapes) {
        Shape out;
        out.tf = Matrix::identity();
        out.add_tag("type", "item");
        out.add_tag("item:name", name);
        
        // Subject is the first component
        out.components.push_back(in);
        
        // Tools are sibling components
        for (const auto& s : shapes) {
            out.components.push_back(s);
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
    
    static std::vector<std::string> argument_keys() { return {"$in", "name", "shapes"}; }
    
    static typename P::json schema() {
        return {
            {"path", "jot/item"},
            {"description", "Converts or wraps the subject shape and optional additional shapes into a single rigid Item."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The base shape."}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "name"}, {"type", "jot:string"}, {"description", "The name of the item."}},
                {{"name", "shapes"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}, {"description", "Additional shapes to bundle inside the item."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ItemPrimitiveOp : P {
    static constexpr const char* path = "jot/Item";
    
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, std::string name, const std::vector<Shape>& shapes) {
        Shape out;
        out.components = shapes;
        out.add_tag("type", "item");
        out.add_tag("item:name", name);
        vfs->write(fulfilling.with_output("$out"), out);
    }
    
    static std::vector<std::string> argument_keys() { return {"name", "shapes"}; }
    
    static typename P::json schema() {
        return {
            {"path", "jot/Item"},
            {"description", "Creates a new rigid Item from a list of shapes with a given name."},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "name"}, {"type", "jot:string"}},
                {{"name", "shapes"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void item_init(fs::VFSNode* vfs) {
    Processor::register_op<ItemOp<>, Shape, std::string, std::vector<Shape>>(vfs, "jot/item");
    Processor::register_op<ItemPrimitiveOp<>, std::string, std::vector<Shape>>(vfs, "jot/Item");
}

} // namespace geo
} // namespace jotcad
