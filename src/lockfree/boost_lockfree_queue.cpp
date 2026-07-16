#include "lockfree_queue.h"
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#include <vector>
#include <atomic>
#include <memory>
#include <cstddef>

namespace {
    struct PoolItem {
        LFMessage msg;
    };

    class BoostLockFreeQueue : public ILockFreeQueue {
    public:
        BoostLockFreeQueue(size_t capacity)
            : queue_(capacity), pool_(capacity), pool_stack_(capacity) {
            // Preallocate pool
            for (size_t i = 0; i < capacity; ++i) {
                pool_[i].msg = LFMessage();
                pool_stack_.push(&pool_[i]);
            }
        }
        bool enqueue(const LFMessage& item) override {
            PoolItem* slot = nullptr;
            if (!pool_stack_.pop(slot)) return false;
            slot->msg = item; // copy
            if (!queue_.push(slot)) {
                pool_stack_.push(slot);
                return false;
            }
            return true;
        }
        bool dequeue(LFMessage& item) override {
            PoolItem* slot = nullptr;
            if (!queue_.pop(slot)) return false;
            item = slot->msg; // copy out
            slot->msg = LFMessage(); // reset
            pool_stack_.push(slot);
            return true;
        }
    private:
        boost::lockfree::queue<PoolItem*> queue_;
        std::vector<PoolItem> pool_;
        boost::lockfree::stack<PoolItem*> pool_stack_;
    };
}

std::unique_ptr<ILockFreeQueue> create_boost_lockfree_queue(size_t capacity) {
    return std::make_unique<BoostLockFreeQueue>(capacity);
}
