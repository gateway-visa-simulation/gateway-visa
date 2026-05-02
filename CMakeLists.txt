cmake_minimum_required(VERSION 3.28)
project(gateway_visa)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependências ---

# OpenSSL
find_package(OpenSSL REQUIRED)

# Spdlog (via FetchContent)
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.9.2
)
FetchContent_MakeAvailable(spdlog)

# Asio (header-only, incluído diretamente)
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third-party/asio/include)

# librdkafka (via FetchContent)
FetchContent_Declare(
    librdkafka
    GIT_REPOSITORY https://github.com/edenhill/librdkafka.git
    GIT_TAG v1.8.2
)
FetchContent_MakeAvailable(librdkafka)

# libpqxx (para PostgreSQL)
FetchContent_Declare(
    libpqxx
    GIT_REPOSITORY https://github.com/libpqxx/libpqxx.git
    GIT_TAG 7.8.0
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

# --- Linkagem das Dependências ---

target_link_libraries(gateway_visa
    # Bibliotecas internas/headers
    asio

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

# --- Diretórios de Inclusão ---

target_include_directories(gateway_visa PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party/asio/include
    ${libpqxx_SOURCE_DIR}/include
    ${librdkafka_SOURCE_DIR}/src-cpp
    ${spdlog_SOURCE_DIR}/include
)
