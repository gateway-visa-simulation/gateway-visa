#pragma once
#include <string>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

class ConfigManager {
public:
    static ConfigManager& getInstance();

    void load(const string& config_path);
    const json& getConfig() const;

private:
    ConfigManager() = default;
    json config_;
};
