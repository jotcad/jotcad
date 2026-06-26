#include "../vfs_node.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace stdfs = std::filesystem;
using namespace fs;

inline void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void test_zenoh_pubsub_direct() {
    std::cout << "[Test] Starting direct Zenoh PubSub test (A <-> B)..." << std::endl;

    VFSNode::Config configA;
    configA.id = "node-A";
    configA.port = 9201;
    configA.storage_dir = "./test_storage_ps_A";
    stdfs::remove_all(configA.storage_dir);
    stdfs::create_directories(configA.storage_dir);
    
    VFSNode::Config configB;
    configB.id = "node-B";
    configB.port = 9202;
    configB.neighbors = {"tcp/127.0.0.1:9201"};
    configB.storage_dir = "./test_storage_ps_B";
    stdfs::remove_all(configB.storage_dir);
    stdfs::create_directories(configB.storage_dir);

    VFSNode nodeA(configA);
    VFSNode nodeB(configB);

    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    json received_payload;

    // Node A listens to test/direct
    nodeA.listen("test/direct", [&](const json& val) {
        std::lock_guard<std::mutex> lock(mtx);
        received = true;
        received_payload = val;
        cv.notify_all();
    });

    // Start Node A listener
    std::thread threadA([&]() {
        nodeA.listen();
    });
    sleep_ms(500); // Allow server to bind

    // Start Node B listener
    std::thread threadB([&]() {
        nodeB.listen();
    });
    sleep_ms(1000); // Allow session handshakes to complete

    // Node B writes to path
    json payload = {{"status", "OK"}, {"value", 100}};
    nodeB.write("test/direct", payload);

    // Wait for propagation
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(3), [&] { return received; });
    }

    assert(received);
    assert(received_payload["value"] == 100);

    // Verify read(path) queries the cached state over Zenoh
    json read_val = nodeB.read("test/direct");
    assert(read_val["value"] == 100);

    std::cout << "✔ Direct Zenoh PubSub (A <- B) & Read successful!" << std::endl;

    nodeA.stop();
    nodeB.stop();

    if (threadA.joinable()) threadA.join();
    if (threadB.joinable()) threadB.join();

    stdfs::remove_all(configA.storage_dir);
    stdfs::remove_all(configB.storage_dir);
}

void test_zenoh_pubsub_multihop() {
    std::cout << "[Test] Starting multi-hop Zenoh PubSub test (A <-> B <-> C)..." << std::endl;

    VFSNode::Config configA;
    configA.id = "node-A";
    configA.port = 9301;
    configA.storage_dir = "./ts_multi_A";
    stdfs::remove_all(configA.storage_dir);
    stdfs::create_directories(configA.storage_dir);

    VFSNode::Config configB;
    configB.id = "node-B";
    configB.port = 9302;
    configB.neighbors = {"tcp/127.0.0.1:9301"};
    configB.storage_dir = "./ts_multi_B";
    stdfs::remove_all(configB.storage_dir);
    stdfs::create_directories(configB.storage_dir);

    VFSNode::Config configC;
    configC.id = "node-C";
    configC.port = 9303;
    configC.neighbors = {"tcp/127.0.0.1:9302"};
    configC.storage_dir = "./ts_multi_C";
    stdfs::remove_all(configC.storage_dir);
    stdfs::create_directories(configC.storage_dir);

    VFSNode nodeA(configA);
    VFSNode nodeB(configB);
    VFSNode nodeC(configC);

    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    json received_payload;

    // Node A listens
    nodeA.listen("test/multi", [&](const json& val) {
        std::lock_guard<std::mutex> lock(mtx);
        received = true;
        received_payload = val;
        cv.notify_all();
    });

    // Start listeners sequentially
    std::thread threadA([&]() { nodeA.listen(); });
    sleep_ms(300);
    std::thread threadB([&]() { nodeB.listen(); });
    sleep_ms(500);
    std::thread threadC([&]() { nodeC.listen(); });
    sleep_ms(1000);

    // Node C writes
    nodeC.write("test/multi", {{"hop", "from-C"}});

    // Wait for propagation
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(4), [&] { return received; });
    }

    assert(received);
    assert(received_payload["hop"] == "from-C");
    std::cout << "✔ Multi-hop Zenoh PubSub (A <- B <- C) successful!" << std::endl;

    nodeA.stop();
    nodeB.stop();
    nodeC.stop();

    if (threadA.joinable()) threadA.join();
    if (threadB.joinable()) threadB.join();
    if (threadC.joinable()) threadC.join();

    stdfs::remove_all(configA.storage_dir);
    stdfs::remove_all(configB.storage_dir);
    stdfs::remove_all(configC.storage_dir);
}

int main() {
    try {
        test_zenoh_pubsub_direct();
        test_zenoh_pubsub_multihop();
        std::cout << "All C++ VFS PubSub tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
