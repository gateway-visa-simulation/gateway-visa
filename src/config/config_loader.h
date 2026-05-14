#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using namespace std;

class ConfigLoader {
public:
    static ConfigLoader& getInstance() {
        static ConfigLoader instance;
        return instance;
    }

    void load(const string& filepath) {
        ifstream file(filepath);
        if (!file.is_open()) {
            throw runtime_error("Could not open config file: " + filepath);
        }
        file >> config_;
    }

    string getDatabaseConnectionString() const {
        return config_["database"]["connection_string"].get<string>();
    }

private:
    ConfigLoader() = default;
    nlohmann::json config_;
};
