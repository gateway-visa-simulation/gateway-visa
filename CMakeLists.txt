cmake_minimum_required(VERSION 3.28)
project(gateway_visa)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependências ---
include(FetchContent)

# OpenSSL
find_package(OpenSSL REQUIRED)

# Spdlog
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.9.2
)
FetchContent_MakeAvailable(spdlog)

# Asio (via FetchContent)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-28-0 # Usando uma tag estável
)
FetchContent_MakeAvailable(asio)

# nlohmann_json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.10.5
)
FetchContent_MakeAvailable(nlohmann_json)

# librdkafka
FetchContent_Declare(
        librdkafka
        GIT_REPOSITORY https://github.com/edenhill/librdkafka.git
        GIT_TAG v1.8.2
)
FetchContent_MakeAvailable(librdkafka)

# libpqxx
find_package(PostgreSQL REQUIRED)
FetchContent_Declare(
        libpqxx
        GIT_REPOSITORY https://github.com/jtv/libpqxx.git
        GIT_TAG 7.9.1
)
FetchContent_MakeAvailable(libpqxx)


# --- Executável Principal ---
add_executable(gateway_visa
        src/main.cpp
        src/common/logger.cpp
        src/config/config_manager.cpp
        src/network/tcp_server.cpp
        src/network/session.cpp
        src/iso8583/iso_parser.cpp
        src/iso8583/iso_builder.cpp
        src/iso8583/iso_message.cpp
        src/iso8583/bitmap.cpp
        src/iso8583/field.cpp
        src/switch/router.cpp
        src/switch/transaction_manager.cpp
        src/switch/circuit_breaker.cpp
        src/issuer/issuer_service.cpp
        src/metrics/metrics_registry.cpp
        src/persistence/transaction_repository.cpp
        src/kafka/kafka_producer.cpp
        src/kafka/kafka_consumer.cpp
        src/mapper/iso_to_json.cpp
        src/mapper/json_to_iso.cpp
        src/security/hsm.cpp
        src/security/crypto.cpp
        src/security/pin_block.cpp
        src/security/tokenization.cpp
        src/security/hsm_service.cpp
        src/database/database_manager.cpp
        src/security/soft_hsm.cpp
)

# --- Diretórios de Inclusão ---
target_include_directories(gateway_visa PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${asio_SOURCE_DIR}/asio/include
        ${nlohmann_json_SOURCE_DIR}/include
        ${PostgreSQL_INCLUDE_DIRS}
        ${libpqxx_SOURCE_DIR}/include
        ${librdkafka_SOURCE_DIR}/src-cpp
        ${spdlog_SOURCE_DIR}/include
)

# --- Linkagem das Dependências ---
target_link_libraries(gateway_visa PRIVATE
        # Spdlog
        spdlog::spdlog

        # librdkafka
        rdkafka++
        ${LIBRDKAFKA_LIBRARIES}

        # libpqxx & libpq
        pqxx
        ${PostgreSQL_LIBRARIES}

        # OpenSSL
        OpenSSL::SSL
        OpenSSL::Crypto
)
