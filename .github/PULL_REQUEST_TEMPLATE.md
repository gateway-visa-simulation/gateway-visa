# Pull Request

## Summary

(Describe the purpose of this PR, what it changes and why.)

## Related Issue

(Reference the issue created for Lock-Free integration or any other relevant tickets)

## Implementation Notes

- Files added/modified:
  - src/lockfree/ (new)
  - src/main.cpp (initialization of global_lockfree_queue)
  - src/... (any router/handler changes)

- API: `ILockFreeQueue` as defined in ISSUE_TEMPLATE.

## How to test

1. Build the project
2. Run unit tests for lockfree implementation
3. Run the gateway and execute client_test.py to validate request/response flow using the lock-free queue

## Checklist

- [ ] Code compiles on CI
- [ ] Unit tests added
- [ ] Benchmark or basic load test added
- [ ] Documentation updated (README, CHANGELOG)

## Notes for reviewers

Pay attention to memory barriers, correct use of atomics, and shutdown ordering for consumer threads.
