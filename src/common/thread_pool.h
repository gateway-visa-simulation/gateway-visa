#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

using namespace std;

class ThreadPool {
public:
    ThreadPool(size_t num_threads) : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    function<void()> task; {
                        unique_lock<mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() { {
            unique_lock<mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (thread &worker: workers_) {
            worker.join();
        }
    }

    template<class F>
    void enqueue(F &&f) { {
            unique_lock<mutex> lock(queue_mutex_);
            tasks_.emplace(forward<F>(f));
        }
        condition_.notify_one();
    }

private:
    vector<thread> workers_;
    queue<function<void()> > tasks_;
    mutex queue_mutex_;
    condition_variable condition_;
    atomic<bool> stop_;
};
