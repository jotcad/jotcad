#include "../vfs_node.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <vector>
#include <chrono>

namespace stdfs = std::filesystem;
using namespace fs;

inline long long GetTimeMS() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// A Bridge that connects two VFSNodes in-memory
struct MemoryBridge : public VFSNode::Connection {
    VFSNode& target_node;
    MemoryBridge(std::string id, VFSNode& target) : target_node(target) {
        neighbor_id = id;
    }

    void notify(const json& selector, const json& payload, const std::vector<std::string>& stack) override {
        target_node.notify(selector, payload, stack);
    }

    void subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) override {
        target_node.subscribe(selector, expiresAt, stack);
    }

    VFSResult read(const VFSNode::VFSRequest& req) override {
        // Not needed for pubsub test
        return {};
    }

    bool is_reverse() const override { return false; }
};

// A simple observer that records notifications
struct TestObserver : public VFSNode::Connection {
    std::vector<json> received_payloads;
    TestObserver(std::string id) { neighbor_id = id; }

    void notify(const json& selector, const json& payload, const std::vector<std::string>& stack) override {
        received_payloads.push_back(payload);
    }

    void subscribe(const json&, long long, const std::vector<std::string>&) override {}
    VFSResult read(const VFSNode::VFSRequest&) override { return {}; }
    bool is_reverse() const override { return false; }
};

void test_pubsub_propagation() {
    VFSNode::Config configA;
    configA.id = "node-A";
    configA.storage_dir = "./test_storage_ps_A";
    stdfs::remove_all(configA.storage_dir);
    stdfs::create_directories(configA.storage_dir);
    
    VFSNode::Config configB;
    configB.id = "node-B";
    configB.storage_dir = "./test_storage_ps_B";
    stdfs::remove_all(configB.storage_dir);
    stdfs::create_directories(configB.storage_dir);

    VFSNode nodeA(configA);
    VFSNode nodeB(configB);

    // Peering: A <-> B
    nodeA.add_connection(std::make_shared<MemoryBridge>("node-B", nodeB));
    nodeB.add_connection(std::make_shared<MemoryBridge>("node-A", nodeA));

    // 1. Node A subscribes to a topic
    // We inject an observer into A to represent the "local" interest
    auto observer = std::make_shared<TestObserver>("local-app");
    nodeA.add_connection(observer);

    Selector topic;
    topic.path = "test/topic";
    
    // Local app expresses interest via Node A
    nodeA.subscribe(topic.to_json(), GetTimeMS() + 10000, {"local-app"});

    // 2. Node B publishes to the topic
    json payload = {{"status", "OK"}, {"value", 42}};
    nodeB.notify(topic.to_json(), payload);

    // 3. Verify Node A's observer got it
    assert(observer->received_payloads.size() == 1);
    assert(observer->received_payloads[0]["value"] == 42);

    std::cout << "✔ C++ PubSub: Notification propagated B -> A -> Observer" << std::endl;

    stdfs::remove_all(configA.storage_dir);
    stdfs::remove_all(configB.storage_dir);
}

void test_multi_hop_routing() {
    VFSNode::Config configA; configA.id = "node-A"; configA.storage_dir = "./ts_multi_A";
    VFSNode::Config configB; configB.id = "node-B"; configB.storage_dir = "./ts_multi_B";
    VFSNode::Config configC; configC.id = "node-C"; configC.storage_dir = "./ts_multi_C";
    stdfs::remove_all(configA.storage_dir); stdfs::create_directories(configA.storage_dir);
    stdfs::remove_all(configB.storage_dir); stdfs::create_directories(configB.storage_dir);
    stdfs::remove_all(configC.storage_dir); stdfs::create_directories(configC.storage_dir);

    VFSNode nodeA(configA); VFSNode nodeB(configB); VFSNode nodeC(configC);

    // Chain: A <-> B <-> C
    nodeA.add_connection(std::make_shared<MemoryBridge>("node-B", nodeB));
    nodeB.add_connection(std::make_shared<MemoryBridge>("node-A", nodeA));
    nodeB.add_connection(std::make_shared<MemoryBridge>("node-C", nodeC));
    nodeC.add_connection(std::make_shared<MemoryBridge>("node-B", nodeB));

    auto observer = std::make_shared<TestObserver>("local-app");
    nodeA.add_connection(observer);

    Selector topic; topic.path = "multi/hop";
    
    // 1. A subscribes (Propagates A -> B -> C)
    nodeA.subscribe(topic.to_json(), GetTimeMS() + 10000, {"local-app"});

    // 2. C notifies (Routes C -> B -> A -> Observer)
    nodeC.notify(topic.to_json(), {{"hop", "from-C"}});

    assert(observer->received_payloads.size() == 1);
    assert(observer->received_payloads[0]["hop"] == "from-C");

    std::cout << "✔ C++ PubSub: Multi-hop routing (3 nodes) successful" << std::endl;

    stdfs::remove_all(configA.storage_dir);
    stdfs::remove_all(configB.storage_dir);
    stdfs::remove_all(configC.storage_dir);
}

void test_interest_reply_catalog() {
    VFSNode::Config configA; configA.id = "node-A"; configA.storage_dir = "./ts_reply_A";
    VFSNode::Config configB; configB.id = "node-B"; configB.storage_dir = "./ts_reply_B";
    stdfs::remove_all(configA.storage_dir); stdfs::create_directories(configA.storage_dir);
    stdfs::remove_all(configB.storage_dir); stdfs::create_directories(configB.storage_dir);

    VFSNode nodeA(configA); VFSNode nodeB(configB);
    nodeB.register_op("test/op", [](const VFSNode::VFSRequest&){});

    nodeA.add_connection(std::make_shared<MemoryBridge>("node-B", nodeB));
    nodeB.add_connection(std::make_shared<MemoryBridge>("node-A", nodeA));

    auto observer = std::make_shared<TestObserver>("local-app");
    nodeA.add_connection(observer);

    // 1. A subscribes to sys/schema
    Selector schemaTopic; schemaTopic.path = "sys/schema";
    nodeA.subscribe(schemaTopic.to_json(), GetTimeMS() + 10000, {"local-app"});

    // 2. Node B should have immediately replied with a CATALOG_ANNOUNCEMENT notification
    bool foundCatalog = false;
    for (const auto& payload : observer->received_payloads) {
        if (payload.contains("type") && payload["type"] == "CATALOG_ANNOUNCEMENT") {
            if (payload.contains("catalog") && payload["catalog"].contains("test/op")) {
                foundCatalog = true;
                break;
            }
        }
    }

    assert(foundCatalog);
    std::cout << "✔ C++ PubSub: Immediate Catalog reply on sys/schema interest" << std::endl;

    stdfs::remove_all(configA.storage_dir);
    stdfs::remove_all(configB.storage_dir);
}

void test_backflow_prevention() {
    VFSNode::Config configA; configA.id = "node-A"; configA.storage_dir = "./ts_loop_A";
    stdfs::remove_all(configA.storage_dir); stdfs::create_directories(configA.storage_dir);
    VFSNode::Config configB; configB.id = "node-B"; configB.storage_dir = "./ts_loop_B";
    stdfs::remove_all(configB.storage_dir); stdfs::create_directories(configB.storage_dir);

    VFSNode nodeA(configA); VFSNode nodeB(configB);

    nodeA.add_connection(std::make_shared<MemoryBridge>("node-B", nodeB));
    nodeB.add_connection(std::make_shared<MemoryBridge>("node-A", nodeA));

    Selector topic; topic.path = "loop/topic";
    nodeA.subscribe(topic.to_json(), GetTimeMS() + 10000, {"node-A"});

    // Should not infinite loop
    nodeA.notify(topic.to_json(), {{"data", 1}});

    std::cout << "✔ C++ PubSub: Loop prevention handled" << std::endl;
    stdfs::remove_all(configA.storage_dir);
    stdfs::remove_all(configB.storage_dir);
}

int main() {
    try {
        test_pubsub_propagation();
        test_multi_hop_routing();
        test_interest_reply_catalog();
        test_backflow_prevention();
        std::cout << "All C++ VFS PubSub tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
