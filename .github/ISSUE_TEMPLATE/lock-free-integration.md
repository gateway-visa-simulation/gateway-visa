---
name: Lock-Free queue integration
about: Request to implement a lock-free queue backend for message exchange (replacing Kafka)
labels: enhancement, help wanted
---

## Goal

Implement a lock-free, in-memory queue backend to replace the previous Kafka producer/consumer flow. The queue will be used to enqueue authorization requests and dequeue responses.

## Suggested API

Provide a header `lockfree_queue.h` exposing:

- class ILockFreeQueue {
  - virtual bool enqueue(const std::string &key, const std::string &payload) = 0;
  - virtual bool try_dequeue(std::string &key, std::string &payload) = 0;
}

A concrete implementation should be placed under `src/lockfree/` (e.g., `ring_buffer.cpp/h`) and registered during app startup as `global_lockfree_queue` (std::shared_ptr).

## Requirements

- Multi-producer safe, single or multi-consumer acceptable.
- Bounded capacity with explicit backpressure behavior (return false on full).
- Low latency and high throughput (target 100k msgs/s on host-grade hardware).
- Minimal allocations: prefer pre-allocated ring buffers or object pools.
- Optional metrics hooks (enqueue/dequeue success/fail counters, size gauge).

## Testing

- Unit tests for enqueue/dequeue under single and multiple producer threads.
- Load test to measure throughput and latency.
- Fault tests: behavior when full, after consumers are slow, and during shutdown.

## Acceptance

- Provide integration branch and PR that:
  - Adds implementation in `src/lockfree/`.
  - Exposes/initializes `global_lockfree_queue` in `main.cpp`.
  - Demonstrates router handlers using the queue (enqueue) and a consumer thread (dequeue).
  - Adds unit and basic load tests.
