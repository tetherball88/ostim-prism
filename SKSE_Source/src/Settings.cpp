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
    toggleFocusKeys.clear();
    toggleInspectorKeys.clear();
    readIntList("Hotkeys", "ToggleFocus", toggleFocusKeys);
    readIntList("Hotkeys", "ToggleInspector", toggleInspectorKeys);

    // [UIBehaviour]
    idleTimeoutSec = GetPrivateProfileIntA("UIBehaviour", "IdleTimeoutSeconds", 30, iniPath.string().c_str());
    if (idleTimeoutSec < 0) idleTimeoutSec = 0;

    enablePortraits = GetPrivateProfileIntA("UIBehaviour", "EnablePortraits", 1, iniPath.string().c_str()) != 0;

    enableGridMenu = GetPrivateProfileIntA("UIBehaviour", "EnableGridMenu", 1, iniPath.string().c_str()) != 0;

    gridColumns = GetPrivateProfileIntA("UIBehaviour", "GridColumns", 4, iniPath.string().c_str());
    if (gridColumns < 1) gridColumns = 1;
    if (gridColumns > 12) gridColumns = 12;

    gridRows = GetPrivateProfileIntA("UIBehaviour", "GridRows", 4, iniPath.string().c_str());
    if (gridRows < 1) gridRows = 1;
    if (gridRows > 12) gridRows = 12;

    mainBlockX = GetPrivateProfileIntA("UIBehaviour", "MainBlockX", -1, iniPath.string().c_str());
    mainBlockY = GetPrivateProfileIntA("UIBehaviour", "MainBlockY", -1, iniPath.string().c_str());
    mainBlockScale = GetPrivateProfileIntA("UIBehaviour", "MainBlockScale", 100, iniPath.string().c_str());
    if (mainBlockScale < 30) mainBlockScale = 30;
    if (mainBlockScale > 250) mainBlockScale = 250;

    excitementBlockX = GetPrivateProfileIntA("UIBehaviour", "ExcitementBlockX", -1, iniPath.string().c_str());
    excitementBlockY = GetPrivateProfileIntA("UIBehaviour", "ExcitementBlockY", -1, iniPath.string().c_str());
    excitementBlockScale = GetPrivateProfileIntA("UIBehaviour", "ExcitementBlockScale", 100, iniPath.string().c_str());
    if (excitementBlockScale < 30) excitementBlockScale = 30;
    if (excitementBlockScale > 250) excitementBlockScale = 250;

    trackedMenus.clear();
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
    SKSE::log::info("Grid menu: {}, columns: {}, rows: {}", enableGridMenu, gridColumns, gridRows);
    SKSE::log::info("Tracked menus: {}", trackedMenus.size());
}

void Settings::SaveLayout(int mainX, int mainY, int mainScale, int exciteX, int exciteY, int exciteScale, int cols, int rows, bool enableGrid, bool enablePortraitsVal) {
    mainBlockX = mainX;
    mainBlockY = mainY;
    mainBlockScale = std::max(30, std::min(250, mainScale));
    excitementBlockX = exciteX;
    excitementBlockY = exciteY;
    excitementBlockScale = std::max(30, std::min(250, exciteScale));
    gridColumns = std::max(1, std::min(12, cols));
    gridRows = std::max(1, std::min(12, rows));
    enableGridMenu = enableGrid;
    enablePortraits = enablePortraitsVal;

    std::filesystem::path iniPath = std::filesystem::current_path() / "Data/SKSE/Plugins/OStimPrism.ini";
    const std::string iniStr = iniPath.string();

    WritePrivateProfileStringA("UIBehaviour", "MainBlockX", std::to_string(mainBlockX).c_str(), iniStr.c_str());
    WritePrivateProfileStringA("UIBehaviour", "MainBlockY", std::to_string(mainBlockY).c_str(), iniStr.c_str());
    WritePrivateProfileStringA("UIBehaviour", "MainBlockScale", std::to_string(mainBlockScale).c_str(), iniStr.c_str());
    WritePrivateProfileStringA("UIBehaviour", "ExcitementBlockX", std::to_string(excitementBlockX).c_str(), iniStr.c_str());
    WritePrivateProfileStringA("UIBehaviour", "ExcitementBlockY", std::to_string(excitementBlockY).c_str(), iniStr.c_str());
    WritePrivateProfileStringA("UIBehaviour", "ExcitementBlockScale", std::to_string(excitementBlockScale).c_str(), iniStr.c_str());
    WritePrivateProfileStringA("UIBehaviour", "GridColumns", std::to_string(gridColumns).c_str(), iniStr.c_str());
    WritePrivateProfileStringA("UIBehaviour", "GridRows", std::to_string(gridRows).c_str(), iniStr.c_str());
    WritePrivateProfileStringA("UIBehaviour", "EnableGridMenu", enableGridMenu ? "1" : "0", iniStr.c_str());
    WritePrivateProfileStringA("UIBehaviour", "EnablePortraits", enablePortraits ? "1" : "0", iniStr.c_str());

    SKSE::log::info("Saved UI layout to {}: Main=({}, {}, {}%), Excitement=({}, {}, {}%), Grid=({}x{}, enabled={}), Portraits={}",
        iniStr, mainBlockX, mainBlockY, mainBlockScale, excitementBlockX, excitementBlockY, excitementBlockScale, gridColumns, gridRows, enableGridMenu, enablePortraits);
}
