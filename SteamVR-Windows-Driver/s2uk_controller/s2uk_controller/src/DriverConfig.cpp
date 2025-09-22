#include "DriverConfig.h"
#include <filesystem>
#include <Windows.h>
#include <fstream>
#include "VRLog.h"

// TODO: full-body tracking toggle, player height parameter.

std::filesystem::path DriverConfig::getDllFilePath(bool& fail) {
    wchar_t path[MAX_PATH];
    fail = false;

    HMODULE hModule = GetModuleHandle(L"driver_s2ukController.dll");
    if (!hModule) {
        fail = true;
        return std::filesystem::path();
    }

    DWORD length = GetModuleFileNameW(hModule, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        fail = true;
        return std::filesystem::path();
    }

    fail = false;
    return std::filesystem::path(path).parent_path().parent_path().parent_path(); // isn't the best solution, but works
}

bool DriverConfig::writeConfig() {
    try {
        nlohmann::json json;
        json["headEMA"] = cfg.headEMA;
        json["leftHandEMA"] = cfg.leftHandEMA;
        json["rightHandEMA"] = cfg.rightHandEMA;
        json["sensorTilt"] = cfg.sensorTilt;

        std::ofstream ofs(cfgPath);
        if (!ofs.is_open()) return false;
        ofs << std::setw(4) << json << '\n';

        LOG("Successfully wrote config.");
        restoredConfig = false; // reset
        return true;
    }
    catch (...) {
        LOG("Failed to write config.");
        return false;
    }
}

bool DriverConfig::readConfig_i(configStruct& out) {
    std::ifstream ifs(cfgPath);
    if (!ifs.is_open()) return false;

    try {
        nlohmann::json json;
        ifs >> json;

        if (!json.contains("headEMA") || !json.contains("leftHandEMA") ||
            !json.contains("rightHandEMA") || !json.contains("sensorTilt")) {
            throw std::runtime_error("Missing keys in config JSON");
        }

        out.headEMA = json["headEMA"].get<double>();
        out.leftHandEMA = json["leftHandEMA"].get<double>();
        out.rightHandEMA = json["rightHandEMA"].get<double>();
        out.sensorTilt = json["sensorTilt"].get<int>();

        LOG("Read config successfully.");

        return true;
    }
    catch (const nlohmann::json::parse_error& e) {
        std::ostringstream oss;
        oss << "[JSON PARSE ERROR] " << e.what()
            << " (byte=" << e.byte << ")";
        LOG(oss.str().c_str());

        if (!restoredConfig) {
            writeConfig();
            restoredConfig = true;
        }
        return false;
    }
    catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "[JSON ERROR] " << e.what();
        LOG(oss.str().c_str());

        if (!restoredConfig) {
            writeConfig();
            restoredConfig = true;
        }
        return false;
    }
}