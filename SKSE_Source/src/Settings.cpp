#include "Settings.h"
#include "PCH.h"
#include <Windows.h>
#include <filesystem>
#include <string>
#include <sstream>

Settings* Settings::GetSingleton() {
    static Settings singleton;
    return &singleton;
}

void Settings::Load() {
    std::filesystem::path iniPath = std::filesystem::current_path() / "Data/SKSE/Plugins/OStimPrism.ini";

    // Helper to read integer list
    auto readIntList = [&](const char* section, const char* key, std::vector<uint32_t>& defaults) {
        char buffer[256];
        GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), iniPath.string().c_str());
        std::string valStr = buffer;
        
        if (valStr.empty()) {
            return; // Keep defaults
        }

        std::vector<uint32_t> keys;
        std::stringstream ss(valStr);
        std::string item;
        while (std::getline(ss, item, ',')) {
            try {
                keys.push_back(std::stoi(item));
            } catch (...) {}
        }
        
        if (!keys.empty()) {
            defaults = keys;
        }
    };

    // [Hotkeys]
    readIntList("Hotkeys", "ToggleFocus", toggleFocusKeys);
    readIntList("Hotkeys", "ToggleInspector", toggleInspectorKeys);

    SKSE::log::info("Settings loaded from {}", iniPath.string());
    SKSE::log::info("Focus Keys: {}", toggleFocusKeys.size());
    SKSE::log::info("Inspector Keys: {}", toggleInspectorKeys.size());
}
