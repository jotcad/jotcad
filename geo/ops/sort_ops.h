#pragma once
#include "protocols.h"
#include "processor.h"
#include "math/interval.h"
#include <algorithm>
#include <map>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct HighestOp : P {
    static constexpr const char* path = "jot/highest";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const fs::Selector& measure, 
                        std::optional<int> bucket, std::optional<double> epsilon_opt, std::optional<double> ratio_opt) {
        if (in.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        struct Item {
            Shape shape;
            double value;
        };
        std::vector<Item> items;
        double min_val = 1e30, max_val = -1e30;

        for (const auto& child : in.components) {
            fs::Selector call = measure;
            call.parameters["$in"] = vfs->materialize(child).value;
            nlohmann::json res = vfs->read<nlohmann::json>(call.with_output("$out"));
            
            double score = 0;
            if (res.is_number()) {
                score = res.get<double>();
            } else if (res.is_array()) {
                score = Interval::from_json(res).max;
            }

            items.push_back({child, score});
            if (score < min_val) min_val = score;
            if (score > max_val) max_val = score;
        }

        double epsilon = epsilon_opt.has_value() ? epsilon_opt.value() : 
                         ratio_opt.has_value() ? (max_val - min_val) * ratio_opt.value() : 1e-4;

        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.value > b.value; });

        std::vector<std::vector<Shape>> buckets;
        if (!items.empty()) {
            buckets.push_back({items[0].shape});
            for (size_t i = 1; i < items.size(); ++i) {
                if (std::abs(items[i].value - items[i-1].value) <= epsilon) buckets.back().push_back(items[i].shape);
                else buckets.push_back({items[i].shape});
            }
        }

        if (bucket.has_value()) {
            int idx = bucket.value();
            Shape out; out.tf = in.tf;
            int real_idx = (idx < 0) ? (int)buckets.size() + idx : idx;
            if (real_idx >= 0 && real_idx < (int)buckets.size()) out.components = buckets[real_idx];
            vfs->write(fulfilling.with_output("$out"), out);
        } else {
            Shape out = in; out.components.clear();
            for (const auto& b : buckets) for (const auto& s : b) out.components.push_back(s);
            vfs->write(fulfilling.with_output("$out"), out);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "measure", "bucket", "epsilon", "ratio"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/highest"},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}}}},
            {"arguments", json::array({
                {{"name", "measure"}, {"type", "jot:op<$in:shape, $out:number>"}},
                {{"name", "bucket"}, {"type", "jot:number"}, {"optional", true}},
                {{"name", "epsilon"}, {"type", "jot:number"}, {"optional", true}},
                {{"name", "ratio"}, {"type", "jot:number"}, {"optional", true}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct LowestOp : P {
    static constexpr const char* path = "jot/lowest";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const fs::Selector& measure, 
                        std::optional<int> bucket, std::optional<double> epsilon_opt, std::optional<double> ratio_opt) {
        if (in.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        struct Item {
            Shape shape;
            double value;
        };
        std::vector<Item> items;
        double min_val = 1e30, max_val = -1e30;

        for (const auto& child : in.components) {
            fs::Selector call = measure;
            call.parameters["$in"] = vfs->materialize(child).value;
            nlohmann::json res = vfs->read<nlohmann::json>(call.with_output("$out"));
            
            double score = 0;
            if (res.is_number()) {
                score = res.get<double>();
            } else if (res.is_array()) {
                score = Interval::from_json(res).min;
            }

            items.push_back({child, score});
            if (score < min_val) min_val = score;
            if (score > max_val) max_val = score;
        }

        double epsilon = epsilon_opt.has_value() ? epsilon_opt.value() : 
                         ratio_opt.has_value() ? (max_val - min_val) * ratio_opt.value() : 1e-4;

        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.value < b.value; });

        std::vector<std::vector<Shape>> buckets;
        if (!items.empty()) {
            buckets.push_back({items[0].shape});
            for (size_t i = 1; i < items.size(); ++i) {
                if (std::abs(items[i].value - items[i-1].value) <= epsilon) buckets.back().push_back(items[i].shape);
                else buckets.push_back({items[i].shape});
            }
        }

        if (bucket.has_value()) {
            int idx = bucket.value();
            Shape out; out.tf = in.tf;
            int real_idx = (idx < 0) ? (int)buckets.size() + idx : idx;
            if (real_idx >= 0 && real_idx < (int)buckets.size()) out.components = buckets[real_idx];
            vfs->write(fulfilling.with_output("$out"), out);
        } else {
            Shape out = in; out.components.clear();
            for (const auto& b : buckets) for (const auto& s : b) out.components.push_back(s);
            vfs->write(fulfilling.with_output("$out"), out);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "measure", "bucket", "epsilon", "ratio"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/lowest"},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}}}},
            {"arguments", json::array({
                {{"name", "measure"}, {"type", "jot:op<$in:shape, $out:number>"}},
                {{"name", "bucket"}, {"type", "jot:number"}, {"optional", true}},
                {{"name", "epsilon"}, {"type", "jot:number"}, {"optional", true}},
                {{"name", "ratio"}, {"type", "jot:number"}, {"optional", true}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void selection_init(fs::VFSNode* vfs) {
    Processor::register_op<HighestOp<>, Shape, fs::Selector, std::optional<int>, std::optional<double>, std::optional<double>>(vfs, "jot/highest");
    Processor::register_op<LowestOp<>, Shape, fs::Selector, std::optional<int>, std::optional<double>, std::optional<double>>(vfs, "jot/lowest");
}

} // namespace geo
} // namespace jotcad
