#pragma once
#include <cstddef>
#include <memory>
#include <string>

// Typed message for lock-free queue
struct LFMessage {
    std::string key;       // STAN or correlation key
    std::string payload;   // JSON (encrypted) payload
};

// Interface for lock-free queue
class ILockFreeQueue {
public:
    virtual ~ILockFreeQueue() = default;
    virtual bool enqueue(const LFMessage& msg) = 0;
    virtual bool dequeue(LFMessage& msg) = 0;
};

// Factory prototype
std::unique_ptr<ILockFreeQueue> create_boost_lockfree_queue(size_t capacity);

// Extern global
extern std::unique_ptr<ILockFreeQueue> global_lockfree_queue;
