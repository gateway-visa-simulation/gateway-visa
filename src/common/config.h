#pragma once
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include "logger.h"

using namespace std;

struct Config {
    string db_connection_string;
    int cache_refresh_interval_sec = 30;
    short server_port = 0;

    static Config load(const string &filename = "config.json") {
        Config config;
        try {
            ifstream file(filename);
            if (file.is_open()) {
                nlohmann::json j;
                file >> j;
                if (j.contains("database") && j["database"].contains("connection_string")) {
                    config.db_connection_string = j["database"]["connection_string"];
                }
                if (j.contains("cache") && j["cache"].contains("refresh_interval_sec")) {
                    config.cache_refresh_interval_sec = j["cache"]["refresh_interval_sec"];
                }
                if (j.contains("server") && j["server"].contains("port")) {
                    config.server_port = j["server"]["port"];
                }
                LOG_INFO("Configuração carregada de {}", filename);
            } else {
                LOG_WARN("Arquivo de configuração {} não encontrado. Usando defaults.", filename);
            }
        } catch (const exception &e) {
            LOG_ERROR("Erro ao carregar configuração: {}", e.what());
        }
        return config;
    }
};