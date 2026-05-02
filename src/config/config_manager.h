#pragma once
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ConfigManager {
public:
    static ConfigManager& getInstance();

    void load(const std::string& config_path);
    const json& getConfig() const;

private:
    ConfigManager() = default;
    json config_;
};
