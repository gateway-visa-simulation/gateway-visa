# Changelog

## Unreleased (2026-07-16)

- Removed Kafka integration and related source files (src/kafka/*).
- Updated CMakeLists to remove librdkafka dependency and excluded kafka sources from build.
- Removed Kafka services from docker-compose.yml and kafka block from config.json.
- DB seed updated: removed KAFKA issuers from db/01_init_schema.sql.
- Updated README(s) with Lock-Free configuration and usage instructions.

Notes: The gateway now processes issuer requests locally (Lock‑Free path). To integrate a specific lock-free queue implementation, provide the interface/signature and it will be added.
