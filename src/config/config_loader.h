#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

class ConfigLoader {
public:
    static ConfigLoader& getInstance() {
        static ConfigLoader instance;
        return instance;
    }

    void load(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open config file: " + filepath);
        }
        file >> config_;
    }

    std::string getDatabaseConnectionString() const {
        return config_["database"]["connection_string"].get<std::string>();
    }

private:
    ConfigLoader() = default;
    nlohmann::json config_;
};
