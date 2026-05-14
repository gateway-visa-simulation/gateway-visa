#include "config_manager.h"
#include "../common/logger.h"
#include <fstream>
#include <stdexcept>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

void ConfigManager::load(const std::string& config_path) {
    ifstream file(config_path);
    if (!file.is_open()) {
        LOG_ERROR("Falha ao abrir arquivo de configuração: {}", config_path);
        throw runtime_error("Configuration file not found");
    }

    try {
        file >> config_;
        LOG_INFO("Configuração carregada com sucesso.");
    } catch (const exception& e) {
        LOG_ERROR("Erro ao fazer o parse do JSON de configuração: {}", e.what());
        throw;
    }
}

const json& ConfigManager::getConfig() const {
    return config_;
}
