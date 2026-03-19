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
    auto readIntList = [&](const char* section, const char* key, std::vector<uint32_t>& out) {
        char buffer[256];
        GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), iniPath.string().c_str());
        std::string valStr = buffer;
        if (valStr.empty()) return;
        std::stringstream ss(valStr);
        std::string item;
        while (std::getline(ss, item, ',')) {
            try { out.push_back(std::stoi(item)); } catch (...) {}
        }
    };

    // Helper to read string list (comma-separated, trims whitespace)
    auto readStringList = [&](const char* section, const char* key, std::vector<std::string>& out) {
        char buffer[1024];
        GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), iniPath.string().c_str());
        std::string valStr = buffer;
        if (valStr.empty()) return;
        std::stringstream ss(valStr);
        std::string item;
        while (std::getline(ss, item, ',')) {
            // Trim leading/trailing whitespace
            auto start = item.find_first_not_of(" \t");
            auto end   = item.find_last_not_of(" \t");
            if (start != std::string::npos)
                out.push_back(item.substr(start, end - start + 1));
        }
    };

    // [Hotkeys]
    readIntList("Hotkeys", "ToggleFocus", toggleFocusKeys);
    readIntList("Hotkeys", "ToggleInspector", toggleInspectorKeys);

    // [UIBehaviour]
    idleTimeoutSec = GetPrivateProfileIntA("UIBehaviour", "IdleTimeoutSeconds", 30, iniPath.string().c_str());
    if (idleTimeoutSec < 0) idleTimeoutSec = 0;

    readStringList("UIBehaviour", "TrackedMenus", trackedMenus);
    // If not set in ini, populate with defaults
    if (trackedMenus.empty()) {
        trackedMenus = {
            "InventoryMenu",
            "ContainerMenu",
            "Journal Menu",
            "MagicMenu",
            "MapMenu",
            "FavoritesMenu",
            "Dialogue Menu",
            "Sleep/Wait Menu",
            "Lockpicking Menu",
            "BarterMenu",
            "GiftMenu",
            "Book Menu",
            "Crafting Menu",
            "RaceSex Menu",
            "Console",
        };
    }

    SKSE::log::info("Settings loaded from {}", iniPath.string());
    SKSE::log::info("Focus Keys: {}", toggleFocusKeys.size());
    SKSE::log::info("Inspector Keys: {}", toggleInspectorKeys.size());
    SKSE::log::info("Idle timeout: {}s", idleTimeoutSec);
    SKSE::log::info("Tracked menus: {}", trackedMenus.size());
}
